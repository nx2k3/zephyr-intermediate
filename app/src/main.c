#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/zbus/zbus.h>
#include <zephyr/task_wdt/task_wdt.h>

LOG_MODULE_REGISTER(demo, LOG_LEVEL_DBG);

#define STACK_SIZE       2048
#define SENSOR_COUNT       18
#define SENSOR_PERIOD_MS  150
#define TEMP_ALARM_MC   27000

#define HEALTH_PERIOD_MS   100
#define QUEUE_WARN_PERCENT  75

/* ===================================================================*/
/*  logger watchdog callback                                                 */
/* ===================================================================*/
static void logger_wdt_cb(int channel_id, void *user_data)
{
    LOG_ERR("[WDT] channel=%d logger timeout", channel_id);
    //k_oops();

}

/* ================================================================== */
/*  Shared channel message                                            */
/* ================================================================== */

struct sensor_data {
    int32_t temperature_mc;
    uint32_t timestamp_ms;
    uint8_t seq;
};

/* Forward declarations required before observer/channel definitions. */
static void display_listener_cb(const struct zbus_channel *chan);

/* ================================================================== */
/*  Observers                                                         */
/* ================================================================== */

ZBUS_LISTENER_DEFINE(display_lis, display_listener_cb);

/*
 * Logger is a message subscriber.
 * It receives message copies, not only channel notifications.
 */
ZBUS_SUBSCRIBER_DEFINE(logger_sub, 8);

/* ================================================================== */
/*  Channel                                                           */
/* ================================================================== */

ZBUS_CHAN_DEFINE(sensor_chan, struct sensor_data,
                 NULL, NULL,
                 ZBUS_OBSERVERS(display_lis, logger_sub),
                 ZBUS_MSG_INIT(.temperature_mc = 0,
                               .timestamp_ms = 0,
                               .seq = 0));

/* ================================================================== */
/*  Listener - synchronous observer                                   */
/* ================================================================== */

static void display_listener_cb(const struct zbus_channel *chan)
{
    const struct sensor_data *msg =
        (const struct sensor_data *)zbus_chan_const_msg(chan);

    /*
     * Listener runs in publisher context.
     * Keep it short. No blocking work here.
     */
    LOG_INF("[DISPLAY-LIS] thread=%s seq=%u temp=%d mC",
            k_thread_name_get(k_current_get()),
            msg->seq,
            msg->temperature_mc);
}

/* ================================================================== */
/*  Publisher                                                         */
/* ================================================================== */

static void sensor_thread_fn(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1); ARG_UNUSED(p2); ARG_UNUSED(p3);

    k_thread_name_set(k_current_get(), "sensor");

    for (int i = 0; i < SENSOR_COUNT; i++) {
        struct sensor_data data = {
            .temperature_mc = 24000 + (i * 350),
            .timestamp_ms = k_uptime_get_32(),
            .seq = (uint8_t)i,
        };

        LOG_INF("[SENSOR] publish seq=%u temp=%d mC",
                data.seq,
                data.temperature_mc);

        int ret = zbus_chan_pub(&sensor_chan, &data, K_MSEC(100));
        if (ret != 0) {
            LOG_WRN("[SENSOR] publish failed ret=%d", ret);
        }

        k_msleep(SENSOR_PERIOD_MS);
    }

    LOG_INF("[SENSOR] done");
}

/* ================================================================== */
/*  Message subscriber - logger                                       */
/* ================================================================== */

static void logger_thread_fn(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1); ARG_UNUSED(p2); ARG_UNUSED(p3);

    int wdt_id = task_wdt_add(1000, logger_wdt_cb, NULL);
    k_thread_name_set(k_current_get(), "logger");

    const struct zbus_channel *chan;
    int received = 0;

    while (received < SENSOR_COUNT) {
        struct sensor_data msg;

        /*
         * Message subscribers receive a copy of the published message.
         * The slow logger will not reread the latest channel value.
         */
        int ret = zbus_sub_wait(&logger_sub, &chan, K_MSEC(1500));
        if (ret != 0) {
            LOG_WRN("[LOGGER-MSG] timeout ret=%d", ret);
            break;
        }

        ret = zbus_chan_read(chan, &msg, K_MSEC(100));
        if (ret != 0) {
            LOG_WRN("[LOGGER-MSG] read failed ret=%d", ret);
            continue;
        }

        received++;

        LOG_INF("[LOGGER-MSG] thread=%s seq=%u temp=%d latency=%ums",
                k_thread_name_get(k_current_get()),
                msg.seq,
                msg.temperature_mc,
                k_uptime_get_32() - msg.timestamp_ms);
        task_wdt_feed(wdt_id);

        /*
         * Slow logger.
         * Message copies let it process old samples safely.
         */
        k_msleep(350);
    }

    task_wdt_delete(wdt_id);
    LOG_INF("[LOGGER-MSG] done received=%d", received);
}

/* ================================================================== */
/*  Subscriber - health-check thread                                  */
/* ================================================================== */
static void health_thread_fn(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1); ARG_UNUSED(p2); ARG_UNUSED(p3);

    k_thread_name_set(k_current_get(), "health-check");

    struct k_msgq *q = (struct k_msgq *)logger_sub.queue;
    bool warned = false;

    while (true) {
        uint32_t used = k_msgq_num_used_get(q);
        uint32_t capacity = k_msgq_num_free_get(q) + used;
        uint8_t percent = (used * 100) / capacity;

        if (percent >= QUEUE_WARN_PERCENT && !warned) {
            LOG_WRN("[HEALTH] alarm queue %u/%u (%u%%)",
                    used, capacity, percent);
            warned = true;
        } else if (percent < QUEUE_WARN_PERCENT) {
            warned = false;
        }

        k_msleep(HEALTH_PERIOD_MS);
    }
}

/* ================================================================== */
/*  Threads                                                           */
/* ================================================================== */

K_THREAD_DEFINE(sensor_thread, STACK_SIZE, sensor_thread_fn,
                NULL, NULL, NULL, 5, 0, 0);

K_THREAD_DEFINE(logger_thread, STACK_SIZE, logger_thread_fn,
                NULL, NULL, NULL, 6, 0, 0);

K_THREAD_DEFINE(health_thread, STACK_SIZE, health_thread_fn,
                NULL, NULL, NULL, 6, 0, 0);

/* ================================================================== */
/*  Main                                                              */
/* ================================================================== */

int main(void)
{
    LOG_INF("=== L4 Demo 2: Zbus Pub-Sub ===");
    LOG_INF("sensor publishes every %dms", SENSOR_PERIOD_MS);
    LOG_INF("display listener runs in publisher context");
    LOG_INF("logger uses message subscriber copies");
    LOG_INF("alarm uses a regular subscriber");
    LOG_INF("alarm threshold: %d mC", TEMP_ALARM_MC);   int ret = task_wdt_init(NULL);

	if (ret != 0) {
		printk("task wdt init failure: %d\n", ret);
		return 0;
	}

    k_sleep(K_MSEC(500));

    return 0;
}
