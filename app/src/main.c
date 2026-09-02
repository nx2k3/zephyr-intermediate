#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <stdbool.h>

LOG_MODULE_REGISTER(homework, LOG_LEVEL_DBG);

#define STACK_SIZE  1024
#define SENSOR_MS   100 /* sensor fires every 100ms */
#define POLL_MS     10  /* polling consumer checks every 10ms */
#define EVENT_COUNT 10  /* total sensor events to produce */

/* Statistics */
static int total_events;
static int total_processed;

static void sensor_handler(struct k_work *work)
{
	ARG_UNUSED(work);
	total_processed++;
	LOG_INF("[HANDLER] processed event %d  tick=%u", total_processed, k_uptime_get_32());
}

K_WORK_DELAYABLE_DEFINE(sensor_work, sensor_handler);

static void sensor_sim_fn(void *p1, void *p2, void *p3)
{
	LOG_INF("[DEBOUNCE] --- simulating 5 rapid button events ---");

	for (int burst = 0; burst < EVENT_COUNT / 5; burst++) {
		for (int i = 0; i < 5; i++) {
			k_msleep(4);
			total_events++;
			LOG_INF("[SENSOR] event %d  tick=%u burst=%d", i, k_uptime_get_32(), burst);

			int ret = k_work_reschedule(&sensor_work, K_MSEC(30));
			if (ret < 0) {
				LOG_ERR("reschedule failed: %d", ret);
			} else {
				LOG_INF("[DEBOUNCE] rescheduled  tick=%u", k_uptime_get_32());
			}
		}

		k_msleep(200);
	}

	LOG_INF("[SENSOR] all events produced");
}

K_THREAD_DEFINE(sensor_thread, STACK_SIZE, sensor_sim_fn, NULL, NULL, NULL, 5, 0, 0);

int main(void)
{
	LOG_INF("=== L3 Debounce Bouns: Burst events with k_work_reschedule ===");
	LOG_INF("With the dealy of 30ms the Sensor fires bursts of 5 events within ~20ms, debounce "
		"delay=30ms");
	LOG_INF("Run this, to verify all the bust events is just one single handler call");

	/* Wait long enough for all events to complete */
	k_msleep((EVENT_COUNT / 5) * (5 * 4 + 200) + 500);
	LOG_INF("\n=== Complete ===");
	return 0;
}
