/*
 * PSDR-F7 Phase 2 / Tier 1: free-running I/Q sampler with UART telemetry.
 *
 * Reads ADC1 (RX_I, PA3) and ADC2 (RX_Q, PA2) as fast as the Zephyr
 * async ADC API will let us, accumulates a windowed RMS on each
 * channel, prints a one-line telemetry summary every 500 ms.
 *
 * No DMA, no hardware timer trigger yet — this is just to prove the
 * channels are alive and the maths is sane. Hook a signal generator
 * to RX_I or RX_Q and you should see RMS track its amplitude.
 */

#include <math.h>
#include <stdint.h>

#include <zephyr/kernel.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/gpio.h>

#define LED0_NODE DT_ALIAS(led0)
static const struct gpio_dt_spec led0 = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

#define ZUSER DT_PATH(zephyr_user)

static const struct adc_dt_spec rx_i = ADC_DT_SPEC_GET_BY_IDX(ZUSER, 0);
static const struct adc_dt_spec rx_q = ADC_DT_SPEC_GET_BY_IDX(ZUSER, 1);

/* Centre count for 12-bit ADC reading 0..3.3V referenced to ~VCC/2. */
#define ADC_MID 2048

/* Telemetry window — at e.g. 50 kSPS this is 25 ksamples per print. */
#define TELEMETRY_PERIOD_MS 500

struct chan_stats {
	uint64_t sumsq;	/* signed-sample squared, accumulated */
	int32_t peak;	/* max |signed sample| within window */
	uint32_t count;	/* samples in this window */
};

static void chan_reset(struct chan_stats *s)
{
	s->sumsq = 0;
	s->peak = 0;
	s->count = 0;
}

static inline void chan_push(struct chan_stats *s, int32_t signed_sample)
{
	int32_t a = signed_sample < 0 ? -signed_sample : signed_sample;
	if (a > s->peak) {
		s->peak = a;
	}
	s->sumsq += (uint64_t)((int64_t)signed_sample * signed_sample);
	s->count++;
}

static float chan_rms(const struct chan_stats *s)
{
	if (s->count == 0) {
		return 0.0f;
	}
	return sqrtf((float)s->sumsq / (float)s->count);
}

static int read_one(const struct adc_dt_spec *spec, int16_t *raw)
{
	struct adc_sequence seq = {
		.buffer = raw,
		.buffer_size = sizeof(*raw),
		.resolution = spec->resolution,
		.channels = BIT(spec->channel_id),
	};

	return adc_read(spec->dev, &seq);
}

int main(void)
{
	int err;

	if (!gpio_is_ready_dt(&led0)) {
		printk("led0 not ready\n");
		return 0;
	}
	gpio_pin_configure_dt(&led0, GPIO_OUTPUT_INACTIVE);

	if (!adc_is_ready_dt(&rx_i) || !adc_is_ready_dt(&rx_q)) {
		printk("adc not ready (i=%d q=%d)\n",
		       adc_is_ready_dt(&rx_i), adc_is_ready_dt(&rx_q));
		return 0;
	}

	err = adc_channel_setup_dt(&rx_i);
	if (err) {
		printk("adc_i setup failed: %d\n", err);
		return 0;
	}
	err = adc_channel_setup_dt(&rx_q);
	if (err) {
		printk("adc_q setup failed: %d\n", err);
		return 0;
	}

	printk("PSDR-F7 Phase 2 / Tier 1 — free-running I/Q sampler\n");
	printk("RX_I = ADC1 ch%u (PA3), RX_Q = ADC2 ch%u (PA2)\n",
	       rx_i.channel_id, rx_q.channel_id);
	printk("sysclk = %u Hz, telemetry every %u ms\n",
	       sys_clock_hw_cycles_per_sec(), TELEMETRY_PERIOD_MS);

	struct chan_stats si, sq;
	chan_reset(&si);
	chan_reset(&sq);

	int64_t window_start = k_uptime_get();
	uint32_t adc_err = 0;

	while (1) {
		int16_t raw_i = 0, raw_q = 0;

		err = read_one(&rx_i, &raw_i);
		if (err) {
			adc_err++;
		} else {
			chan_push(&si, (int32_t)raw_i - ADC_MID);
		}

		err = read_one(&rx_q, &raw_q);
		if (err) {
			adc_err++;
		} else {
			chan_push(&sq, (int32_t)raw_q - ADC_MID);
		}

		int64_t now = k_uptime_get();
		if (now - window_start >= TELEMETRY_PERIOD_MS) {
			uint32_t elapsed_ms = (uint32_t)(now - window_start);
			uint32_t rate_hz = elapsed_ms ?
				(uint32_t)((uint64_t)si.count * 1000U / elapsed_ms) : 0;

			printk("[%lld] rate=%uHz  I rms=%.1f peak=%d  Q rms=%.1f peak=%d  err=%u\n",
			       now, rate_hz,
			       (double)chan_rms(&si), si.peak,
			       (double)chan_rms(&sq), sq.peak,
			       adc_err);

			gpio_pin_toggle_dt(&led0);
			chan_reset(&si);
			chan_reset(&sq);
			window_start = now;
		}
	}

	return 0;
}
