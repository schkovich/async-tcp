#include <Arduino.h>
#include "pico/async_context_threadsafe_background.h"

bool core1_separate_stack = true;

volatile bool operational = false;
volatile bool serial_ready = false;
static volatile uint64_t timestamp_exit = 0;
static volatile uint64_t timestamp_enter = 0;

static async_context_threadsafe_background_t asyncCtx;

uint32_t doSomeWork(void* param) {
    const auto value = static_cast<uint32_t*>(param);
    (*value)++;
    // Log timestamp right before sem_release will happen
    if (const auto worker = static_cast<async_when_pending_worker*>(asyncCtx.core.when_pending_list)) {
        Serial1.printf("[INFO][%u][%llu] Pre-sem_release worker state:\n"
                      "  address: %p\n"
                      "  next: %p\n"
                      "  do_work: %p\n"
                      "  work_pending: %d\n"
                      "  value: %d\n",
            get_core_num(),
            to_us_since_boot(get_absolute_time()),
            worker,
            worker->next,
            worker->do_work,
            worker->work_pending,
            *value);
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

    RP2040::enableDoubleResetBootloader();
    serial_ready = true;
    while (!operational) {
        delay(10);
    }

    Serial1.printf("C0 ready...\n");
}

void setup1() {
    while (!serial_ready) {
        delay(10);
    }
    async_context_threadsafe_background_config_t cfg = async_context_threadsafe_background_default_config();
    operational = async_context_threadsafe_background_init(&asyncCtx, &cfg);
    assert(operational);
    Serial1.printf("C1 ready...\n");
}

void loop() {
    static unsigned long c0_counter = 0;
    static uint32_t myNumber = 0;

    if (c0_counter % 111 == 0) {
        timestamp_enter = to_us_since_boot(get_absolute_time());
        const auto rc = async_context_execute_sync(&asyncCtx.core, doSomeWork, &myNumber);
        assert(rc == myNumber);
        timestamp_exit = to_us_since_boot(get_absolute_time());
        Serial1.printf("value: %d; enter: %llu; exit: %llu\n", rc, timestamp_enter, timestamp_exit);
    }
    c0_counter++;
    delay(1);
}

void loop1() {
    tight_loop_contents();
}
