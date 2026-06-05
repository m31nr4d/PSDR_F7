#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

#define LED0_NODE DT_ALIAS(led0)

static const struct gpio_dt_spec led0 = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

int main(void)
{
	if (!gpio_is_ready_dt(&led0)) {
		printk("led0 not ready\n");
		return 0;
	}

	gpio_pin_configure_dt(&led0, GPIO_OUTPUT_INACTIVE);

	printk("PSDR-F7 alive, sysclk=%u Hz\n", sys_clock_hw_cycles_per_sec());

	uint32_t tick = 0;

	while (1) {
		gpio_pin_toggle_dt(&led0);
		k_msleep(500);
		if ((tick++ & 0x07) == 0) {
			printk("[%u] uptime=%u ms\n", tick, k_uptime_get_32());
		}
	}

	return 0;
}
