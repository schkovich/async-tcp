/*
 * AsyncTCPClient Example Implementation
 *
 * This file demonstrates the use of the AsyncTCPClient library to connect to a
 * Quote of the Day (QOTD) server and an Echo server asynchronously.
 *
 * It showcases:
 * - Proper thread safety using SyncBridge for shared resources
 * - Event handling with EventBridge derivatives
 * - Core affinity management for non-thread-safe operations
 * - Asynchronous networking on a dual-core Raspberry Pi Pico
 */

#ifndef ESPHOSTSPI
#error This example requires an ESP-Hosted-FG WiFi chip to be defined, see the documentation
// For example, add this to your boards.local.txt:
// rpipico.build.extra_flags=-DESPHOST_RESET=D5 -DESPHOST_HANDSHAKE=D7 -DESPHOST_DATA_READY=D6 -DESPHOST_CS=D1 -DESPHOSTSPI=SPI
#endif

#include <WiFi.h>
#include <algorithm>
#include <iostream>
#include "../include/e5/EchoConnectedHandler.hpp"
#include "../include/e5/EchoReceivedHandler.hpp"
#include "../include/e5/IoWrite.hpp"
#include "../include/e5/QotdConnectedHandler.hpp"
#include "../include/e5/QotdReceivedHandler.hpp"
#include "../include/e5/QuoteBuffer.hpp"
#include "AsyncTcpClient.hpp"
#include "ContextManager.hpp"
#include "SerialPrinter.hpp"
#include "secrets.h" // Contains STASSID, STAPSK, QOTD_HOST, ECHO_HOST, QOTD_PORT, ECHO_PORT

/**
 * Allocate separate 8KB stack for core1
 *
 * When false: 8KB stack is split between cores (4KB each)
 * When true:  Each core gets its own 8KB stack
 *
 * Required for reliable dual-core operation with network stack
 * and temperature monitoring running on separate cores.
 */
bool core1_separate_stack = true;

volatile bool operational = false;  // Global flag for core synchronization
volatile bool ctx1_ready = false;   // For loop() to wait for setup1()


// WiFi credentials from secrets.h
const auto *ssid = STASSID;
const auto *password = STAPSK;
WiFiMulti multi;

// Server details
const auto *qotd_host = QOTD_HOST;
const auto *echo_host = ECHO_HOST;
constexpr uint16_t qotd_port = QOTD_PORT;  // QOTD service port
constexpr uint16_t echo_port = ECHO_PORT;  // Echo service port

// TCP clients
AsyncTcp::AsyncTcpClient qotd_client;
AsyncTcp::AsyncTcpClient echo_client;

// IP addresses (resolved once at startup)
IPAddress qotd_ip_address;
IPAddress echo_ip_address;

// Timing variables
unsigned long previous_red = 0;   // Last time QOTD was requested
unsigned long previous_yellow = 0;    // Last time echo was sent
unsigned long previous_blue = 0;   // Last time heap stats were printed

// Constants for intervals
constexpr long red_interval = 5555;       // Interval for QOTD requests (milliseconds)
constexpr long yellow_interval = 3333;  // Interval for echo requests (milliseconds)
constexpr long blue_interval = 11111; // Interval for heap stats (milliseconds)

// Global asynchronous context managers for each core
auto ctx0 = std::make_unique<AsyncTcp::ContextManager>();  // Core 0
auto ctx1 = std::make_unique<AsyncTcp::ContextManager>(); // Core 1

// Thread-safe buffer for storing the quote
e5::QuoteBuffer qotd_buffer(ctx0);

constexpr int MAX_QOTD_SIZE = 512;

/**
 * @brief Connects to the "quote of the day" server and initiates a connection.
 *
 * This function attempts to connect to the server using the pre-resolved IP address.
 */
void get_quote_of_the_day() {
    if (0 == qotd_client.connect(qotd_ip_address, qotd_port)) {
        DEBUGV("Failed to connect to QOTD server.\n");
    }
}

/**
 * @brief Connects to the echo server and sends data if available.
 *
 * This function checks if the echo client is connected. If not, it attempts to connect.
 * If connected and there is data in the transmission buffer, it sends the data to the server.
 */
void get_echo() {
    if (!echo_client.connected()) {
        if (0 == echo_client.connect(echo_ip_address, echo_port)) {
            DEBUGV("Failed to connect to echo server..\n");
        }
    } else {
        // Get the quote buffer content in a thread-safe manner
        const std::string buffer_content = qotd_buffer.get();

        if (!buffer_content.empty()) {
            // Use IoWrite for thread-safe write operations
            e5::IoWrite io_write(ctx0, echo_client);
            io_write.write(buffer_content.c_str());
        } else {
            DEBUGV("Nothing to send to echo server.\n");
        }
    }
}

/**
 * @brief Prints heap statistics using the SerialPrinter.
 */
extern "C" void print_heap_stats(AsyncTcp::ContextManagerPtr& ctx) {
    // Get heap data
    const int freeHeap = rp2040.getFreeHeap();
    const int usedHeap = rp2040.getUsedHeap();
    const int totalHeap = rp2040.getTotalHeap();

    // Format the string with stats
    char print_me[64];
    const auto print_me_length =
            snprintf(print_me, sizeof(print_me), "Free: %d, Used: %d, Total: %d", freeHeap, usedHeap, totalHeap);
    (void) print_me_length;

    e5::SerialPrinter serial_printer(ctx);
    serial_printer.print(print_me);
}

/**
 * @brief Initializes the Wi-Fi connection and asynchronous context on Core 0.
 */
[[maybe_unused]] void setup() {
    Serial.begin(115200);
    while(!Serial) {
        delay(10);
    }
    delay(5000);
    DEBUGV("C0: Blue leader standing by...\n");
    RP2040::enableDoubleResetBootloader();

    // Connect to Wi-Fi network
    DEBUGV("Connecting to %s\n", ssid);
    multi.addAP(ssid, password);

    if (multi.run() != WL_CONNECTED) {
        DEBUGV("Unable to connect to network, rebooting in 10 seconds...\n");
        delay(10000);
        rp2040.reboot();
    }

    Serial.println("Wi-Fi connected");

    // Resolve host names to IP addresses once at startup
    hostByName(qotd_host, qotd_ip_address, 2000);
    hostByName(echo_host, echo_ip_address, 2000);

    // Initialize context on Core 0
    if (!ctx0->initDefaultContext()) {
        DEBUGV("ctx0 init failed on the Core 0\n");
    }
    DEBUGV("Core %d\n", ctx0->getCore());

    // Set up event handlers
    e5::SerialPrinter serial_printer(ctx1);

    // Echo client handlers
    auto echo_connected_handler = std::make_unique<e5::EchoConnectedHandler>(ctx0, echo_client, serial_printer);
    echo_client.setOnConnectedCallback(std::move(echo_connected_handler));

    auto echo_received_handler = std::make_unique<e5::EchoReceivedHandler>(ctx0, echo_client, serial_printer);
    echo_client.setOnReceivedCallback(std::move(echo_received_handler));

    // QOTD client handlers with thread-safe buffer
    auto qotd_connected_handler = std::make_unique<e5::QotdConnectedHandler>(ctx0, qotd_client, serial_printer);
    qotd_client.setOnConnectedCallback(std::move(qotd_connected_handler));

    auto qotd_received_handler = std::make_unique<e5::QotdReceivedHandler>(ctx0, qotd_buffer, qotd_client);
    qotd_client.setOnReceivedCallback(std::move(qotd_received_handler));

    operational = true;
}

/**
 * @brief Initializes the asynchronous context on Core 1.
 */
[[maybe_unused]] void setup1() {
    while (!operational) {
        delay(10);
    }

    DEBUGV("C1: Red leader standing by...\n");

    if (!ctx1->initDefaultContext()) {
        DEBUGV("CTX init failed on Core 1\n");
    }
    DEBUGV("Core %d\n", ctx1->getCore());
    ctx1_ready = true;
}

/**
 * @brief Main loop function on Core 0.
 * Handles periodic requests to the QOTD and echo servers.
 */
[[maybe_unused]] void loop() {

    if (!ctx1_ready) {
        delay(10);
        return;
    }

    const unsigned long currentMillis = millis();

    // Check if it's time to request a new quote
    if (currentMillis - previous_red >= red_interval) {
        previous_red = currentMillis;
        get_quote_of_the_day();
    }

    // Check if it's time to send data to echo server
    if (currentMillis - previous_yellow >= yellow_interval) {
        previous_yellow = currentMillis;
        get_echo();
    }

    // Check if it's time to print heap statistics
    if (currentMillis - previous_blue >= blue_interval) {
        previous_blue = currentMillis;
        print_heap_stats(ctx1);
    }
}

/**
 * @brief Loop function for Core 1.
 * Currently empty as all work is handled through the async context.
 */
[[maybe_unused]] void loop1() {
    // Core 1 work is handled through the async context
}