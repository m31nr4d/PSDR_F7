/*
 * Port of the original PSDR DSP routines onto CMSIS-DSP + Zephyr.
 *
 * Functions kept in 1:1 correspondence with the original main.c so
 * that the numerical behaviour stays comparable. Bug fixes are
 * called out inline.
 */

#include "dsp.h"

#include <arm_math.h>

static float fft_filter_coef[PSDR_FFT_BUFLEN];
static float filter_temp[PSDR_FFT_BUFLEN];

void psdr_dsp_init(void)
{
	for (unsigned int i = 0; i < PSDR_FFT_BUFLEN; i++) {
		fft_filter_coef[i] = 0.0f;
		filter_temp[i] = 0.0f;
	}
}

void psdr_dsp_set_filter(int bandwidth, enum psdr_sideband sb, int offset)
{
	const int N = (int)PSDR_FFT_BUFLEN;

	for (int i = 0; i < N; i++) {
		int v = 0;
		switch (sb) {
		case PSDR_SIDEBAND_LSB:
			if ((i > N - (offset + bandwidth)) && (i < N - offset)) {
				v = 1;
			}
			break;
		case PSDR_SIDEBAND_USB:
			if ((i > offset) && (i < offset + bandwidth)) {
				v = 1;
			}
			break;
		case PSDR_SIDEBAND_AM:
			if (((i > N - (offset + bandwidth)) && (i < N - offset))
			    || ((i > offset) && (i < offset + bandwidth))) {
				v = 1;
			}
			break;
		}
		fft_filter_coef[i] = (float)v;
	}
	fft_filter_coef[PSDR_FFT_SIZE] = 0.0f;
	fft_filter_coef[PSDR_FFT_BUFLEN - 1] = 0.0f;
}

void psdr_dsp_apply_filter(float *samples, int shift)
{
	const int N = (int)PSDR_FFT_SIZE;

	/* Real-scalar multiply: filter_temp[2i..] = samples[2i..] * coef[i]
	 * fft_filter_coef is a real mask (0.0 or 1.0 per bin) written by
	 * psdr_dsp_set_filter with stride 1, NOT interleaved complex.
	 * Reading coef[i*2+1] as 'ci' was wrong — it pulled a mask value
	 * for a neighbouring bin and corrupted both output components.
	 * Applying the real-mask convention also eliminates the imaginary-part
	 * sign error noted in the earlier BUG-PORT comment (ci was never a
	 * genuine imaginary coefficient). */
	for (int i = 0; i < N; i++) {
		float sr = samples[i * 2];
		float si = samples[i * 2 + 1];
		float cr = fft_filter_coef[i * 2];

		filter_temp[i * 2]     = sr * cr;
		filter_temp[i * 2 + 1] = si * cr;
	}

	const int B = (int)PSDR_FFT_BUFLEN;
	for (int i = 0; i < B; i++) {
		int src = i + 2 * shift;
		if (src < 0 || src >= B) {
			samples[i] = 0.0f;
		} else {
			samples[i] = filter_temp[src];
		}
	}
}

float psdr_dsp_rms(const float *samples, int length)
{
	if (length <= 0) {
		return 0.0f;
	}

	float acc = 0.0f;
	for (int i = 0; i < length; i++) {
		acc += samples[i] * samples[i];
	}
	acc /= (float)length;

	float result;
	arm_sqrt_f32(acc, &result);
	return result;
}

void psdr_dsp_zero_with_overlap(float *bank, const float *overlap,
				int filter_kernel_len)
{
	const int B = (int)PSDR_FFT_BUFLEN;
	int copy = filter_kernel_len * 2;

	if (copy < 0) {
		copy = 0;
	}
	if (copy > B) {
		copy = B;
	}
	for (int i = 0; i < copy; i++) {
		bank[i] = overlap[i];
	}
	for (int i = copy; i < B; i++) {
		bank[i] = 0.0f;
	}
}

void psdr_dsp_fft_impulse(float *samples, int bin)
{
	for (unsigned int i = 0; i < PSDR_FFT_BUFLEN; i++) {
		samples[i] = 0.0f;
	}
	if (bin < 0 || (unsigned int)bin >= PSDR_FFT_SIZE) {
		return;
	}
	samples[bin * 2]     = 0.9f;
	samples[bin * 2 + 1] = 0.9f;
}

void psdr_dsp_process(float *bank, int if_shift, enum psdr_sideband sb,
		      int am_demod)
{
	arm_cfft_radix4_instance_f32 fft;

	arm_cfft_radix4_init_f32(&fft, PSDR_FFT_SIZE, 0, 1);
	arm_cfft_radix4_f32(&fft, bank);

	psdr_dsp_apply_filter(bank, if_shift);
	(void)sb; /* sideband is currently encoded in the filter mask */

	arm_cfft_radix4_init_f32(&fft, PSDR_FFT_SIZE, 1, 1);
	arm_cfft_radix4_f32(&fft, bank);

	if (am_demod) {
		float mag[PSDR_FFT_SIZE];
		arm_cmplx_mag_f32(bank, mag, PSDR_FFT_SIZE);
		for (unsigned int i = 0; i < PSDR_FFT_SIZE; i++) {
			bank[i * 2]     = mag[i];
			bank[i * 2 + 1] = mag[i];
		}
	}
}

float psdr_dsp_self_test(void)
{
	static float scratch[PSDR_FFT_BUFLEN];
	arm_cfft_radix4_instance_f32 fft;

	/* Frequency-domain impulse in bin 10, then inverse FFT.
	 * The time-domain output is a complex tone whose RMS should be
	 * a deterministic constant for a given CMSIS-DSP build. */
	psdr_dsp_fft_impulse(scratch, 10);

	arm_cfft_radix4_init_f32(&fft, PSDR_FFT_SIZE, 1, 1);
	arm_cfft_radix4_f32(&fft, scratch);

	return psdr_dsp_rms(scratch, (int)PSDR_FFT_BUFLEN);
}
