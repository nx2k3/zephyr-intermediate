#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(demo, LOG_LEVEL_DBG);

#define STACK_SIZE (1024U)

#define PRIO_COOP (-1)
#define PRIO_HIGH (3U)
#define PRIO_MED  (5U)
#define PRIO_LOW  (7u)

#define ITER_COOP  (5U)
#define SLEEP_HIGH (100U)
#define SLEEP_MED  (200U)
#define SLEEP_LOW  (300U)

void t_low_fn(void *p1, void *p2, void *p3)
{
	while (1) {
		LOG_INF("T_LOW running");
		k_msleep(SLEEP_LOW);
	}
}

void t_med_fn(void *p1, void *p2, void *p3)
{
	while (1) {
		LOG_INF("T_MED running");
		k_msleep(SLEEP_MED);
	}
}

void t_high_fn(void *p1, void *p2, void *p3)
{
	while (1) {
		LOG_INF("T_HIGH running");
		k_msleep(SLEEP_HIGH);
	}
}

void t_coop_fn(void *p1, void *p2, void *p3)
{
	LOG_INF("T_COOP starting with %d busy iteration", ITER_COOP);

	for (int i = 0; i < ITER_COOP; i++) {
		k_busy_wait(40000);
		LOG_INF("T_COOP %d/%d step", i, ITER_COOP);
	}

	LOG_INF("T_COOP yielding now");
	k_yield();

	LOG_INF("T_COOP done");
}

K_THREAD_DEFINE(thread_low, STACK_SIZE, t_low_fn, NULL, NULL, NULL, PRIO_LOW, 0, 0);
K_THREAD_DEFINE(thread_med, STACK_SIZE, t_med_fn, NULL, NULL, NULL, PRIO_MED, 0, 0);
K_THREAD_DEFINE(thread_high, STACK_SIZE, t_high_fn, NULL, NULL, NULL, PRIO_HIGH, 0, 0);
K_THREAD_DEFINE(thread_coop, STACK_SIZE, t_coop_fn, NULL, NULL, NULL, PRIO_COOP, 0, 0);

int main(void)
{
	LOG_INF("=== L1 Task 1: Kernal and Scheduling ===");
	LOG_INF("T_LOW\tpriority=%d\tsleeps for %dms", PRIO_LOW, SLEEP_LOW);
	LOG_INF("T_MED\tpriority=%d\tsleeps for %dms", PRIO_MED, SLEEP_MED);
	LOG_INF("T_HIGH\tpriority=%d\tsleeps for %dms", PRIO_HIGH, SLEEP_HIGH);
	LOG_INF("T_COOP\tpriority=%d\t%d busy iteration then yield", PRIO_COOP, ITER_COOP);

	return 0;
}
