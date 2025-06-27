#include "pico/async_context_threadsafe_background.h"
#include <Arduino.h>

bool core1_separate_stack = true;
static async_context_threadsafe_background_t asyncCtx;
static volatile bool operational = false;
static volatile uint64_t timestamp_exit = 0;
static volatile uint64_t timestamp_enter = 0;

uint32_t doSomeWork(void *param) {
    const auto value = static_cast<uint32_t *>(param);
    (*value)++;
    // Log timestamp right before sem_release will happen
    if (const auto worker = static_cast<async_when_pending_worker *>(
            asyncCtx.core.when_pending_list)) {
        worker->user_data = value; // Store value pointer as canary
        Serial1.printf("[INFO][%d][%llu] Pre-sem_release worker state:\n"
                       "  address: %p\n"
                       "  next: %p\n"
                       "  do_work: %p\n"
                       "  work_pending: %d\n"
                       "  user_data: %p (value: %d)\n",
                       get_core_num(), to_us_since_boot(get_absolute_time()),
                       worker, worker->next, worker->do_work,
                       worker->work_pending, worker->user_data, *value);
    }

    return *value;
}

void setup() {
    Serial1.setRX(PIN_SERIAL1_RX);
    Serial1.setTX(PIN_SERIAL1_TX);
    Serial1.setPollingMode(true);
    Serial1.begin(115200);

    while (!Serial1) {
        delay(10);
    }

    // Initialize asyncCtx on core 0
    async_context_threadsafe_background_config_t cfg =
        async_context_threadsafe_background_default_config();
    operational = async_context_threadsafe_background_init(&asyncCtx, &cfg);

    Serial1.printf("Core 0 ready\n");
}

void setup1() {
    while (!operational) {
        delay(10);
    }
    Serial1.printf("Core 1 ready\n");
}

void loop() { tight_loop_contents(); }

void loop1() {
    delay(5);
    static uint32_t value = 0;

    timestamp_enter = to_us_since_boot(get_absolute_time());
    const uint32_t rc =
        async_context_execute_sync(&asyncCtx.core, doSomeWork, &value);
    timestamp_exit = to_us_since_boot(get_absolute_time());

    // Quick check of list state right after return
    if (const auto worker = static_cast<async_when_pending_worker *>(
            asyncCtx.core.when_pending_list)) {
        worker->user_data = reinterpret_cast<void *>(
            timestamp_exit); // Store return timestamp as canary
        Serial1.printf("[POST-SYNC] worker:%p next:%p do_work:%p\n", worker,
                       worker->next, worker->do_work);
    }

    Serial1.printf("[INFO][%d][%llu] execute_sync returned %d in %llu us\n",
                   get_core_num(), timestamp_exit, rc,
                   timestamp_exit - timestamp_enter);

    if (const auto worker1 = static_cast<async_when_pending_worker *>(
            asyncCtx.core.when_pending_list)) {
        Serial1.printf(
            "[NO-DELAY] worker:%p next:%p do_work:%p user_data: %llu\n",
            worker1, worker1->next, worker1->do_work,
            reinterpret_cast<uint64_t>(worker1->user_data));
    }
    delay(7);
}
