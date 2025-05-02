/**
 * @file execute_sync_patch_test.cpp
 * @brief Test script to showcase a [bug](https://github.com/raspberrypi/pico-sdk/issues/2433)
 * in the Pico SDK's async_context_execute_sync functionality
 * 
 * This test demonstrates a bug in the Pico SDK where cross-core synchronous execution
 * can lead to issues when memory operations are performed after the sync call.
 * 
 * The test uses two cores:
 * - Core 0: Handles WiFi connection and periodically calls a function that executes on Core 1
 * - Core 1: Initializes the async context and handles cross-core execution requests
 * 
 * The key test occurs in the green_call() function, which:
 * 1. Performs a sync_call() to execute doSomeWork() on Core 1
 * 2. Allocates and fills a large array immediately after the sync call returns
 * 
 * This sequence was found to trigger a bug in earlier versions of the Pico SDK,
 * particularly when memory operations follow a cross-core synchronous execution.
 */

#include <Arduino.h>
#include <array>
#include <memory>
#include "pico/async_context_threadsafe_background.h"
#include "secrets.h"
#include "../../arduino-pico/libraries/WiFi/src/WiFi.h"

// Enable Core 1 to have a separate stack (recommended for stability)
bool core1_separate_stack = true;

// Flag to indicate when the async context is operational
volatile bool operational = false;

// WiFi credentials from secrets.h
const auto* ssid = STASSID;
const auto* password = STAPSK;
WiFiMulti multi;

// The async context that enables cross-core communication
static async_context_threadsafe_background_t asyncCtx;

/**
 * @brief Simple work function that increments a counter
 * 
 * This function is executed on Core 1 when called via async_context_execute_sync.
 * It demonstrates the most basic operation that can be performed cross-core.
 * 
 * @param param Pointer to the counter that will be incremented
 * @return The new value of the counter after incrementing
 */
uint32_t doSomeWork(void* param) {
    auto* ptr = static_cast<uint32_t*>(param);
    (*ptr) += 1; // Simple operation to demonstrate synchronous call
    return *ptr;
}

/**
 * @brief Wrapper to call doSomeWork() synchronously on Core 1
 * 
 * Uses async_context_execute_sync to execute the doSomeWork function
 * on Core 1, regardless of which core this function is called from.
 * 
 * @param myNumber Reference to the counter that will be incremented
 * @return The new value of the counter after incrementing
 */
uint32_t sync_call(uint32_t& myNumber) {
    return async_context_execute_sync(&asyncCtx.core, doSomeWork, &myNumber);
}

/**
 * @brief Test function that demonstrates the bug
 * 
 * This function performs two key operations:
 * 1. Calls sync_call() which executes on Core 1
 * 2. Allocates and fills a large array immediately afterward
 * 
 * The combination of cross-core execution followed by memory operations
 * was found to trigger a bug in earlier versions of the Pico SDK.
 * 
 * @param myNumber Reference to the counter that will be incremented
 * @return The result from sync_call()
 */
uint32_t green_call(uint32_t& myNumber) {
    static constexpr size_t BIGNUMBER = 4096;
    std::array<uint8_t, BIGNUMBER> arr;
    const uint32_t rc = sync_call(myNumber);
    std::uninitialized_fill_n(arr.begin(), BIGNUMBER, 1);
    return rc;
}

/**
 * @brief Setup function for Core 0
 * 
 * Initializes serial communication, connects to WiFi,
 * and waits for Core 1 to be operational.
 */
void setup() {
    Serial1.setRX(PIN_SERIAL1_RX);
    Serial1.setTX(PIN_SERIAL1_TX);
    Serial1.begin(115200);
    while (!Serial1) {
        delay(10);
    }
    // Connect to Wi-Fi network
    DEBUGV("Connecting to %s\n", ssid);
    multi.addAP(ssid, password);

    if (multi.run() != WL_CONNECTED) {
        DEBUGV("Unable to connect to network, rebooting in 10 seconds...\n");
        delay(10000);
        rp2040.reboot();
    }
    Serial.println("Wi-Fi connected");
    while (!operational) {
        delay(10);
    }
    RP2040::enableDoubleResetBootloader();
    pinMode(LED_BUILTIN, OUTPUT);
    Serial.printf("C0 ready...\n");
}

/**
 * @brief Setup function for Core 1
 * 
 * Initializes the async context that enables cross-core execution.
 * This must be done on Core 1 to properly handle request execution.
 */
void setup1() {
    async_context_threadsafe_background_config_t cfg = async_context_threadsafe_background_default_config();
    operational = async_context_threadsafe_background_init(&asyncCtx, &cfg);
    assert(operational);
    Serial.printf("C1 ready...\n");
}

/**
 * @brief Main loop for Core 0
 * 
 * Periodically calls green_call() to test cross-core execution,
 * and reports stack and heap statistics.
 */
void loop() {
    delay(2);
    static unsigned long c0_counter = 0;
    static uint32_t myNumber = 0;
    if (c0_counter % 333 == 0) {
        const auto rc = green_call(myNumber);
        Serial.printf("Core %d: doSomeWork() returned %d\n", get_core_num(), rc);
    } else if (c0_counter % 5050 == 0) {
        Serial.printf("Core %d: Free stack %d bytes\n", get_core_num(), rp2040.getFreeStack());
    } else if (c0_counter % 9090 == 0) {
        Serial.printf("Free heap: %d bytes\n", rp2040.getFreeHeap());
    }
    c0_counter++;
    delay(2);
}

/**
 * @brief Main loop for Core 1
 * 
 * Handles async context processing internally (via background worker)
 * and periodically reports stack statistics.
 */
void loop1() {
    delay(2);
    static unsigned long c1_counter = 0;
    if (c1_counter % 7070 == 0) {
        Serial.printf("Core %d: Free stack %d bytes\n", get_core_num(), rp2040.getFreeStack());
    }
    c1_counter++;
    delay(2);
}
