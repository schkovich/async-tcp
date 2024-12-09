/*
    main.cpp
    This file contains the implementation of a TCP client that connects to a "quote of the day" service and an echo server.
    It demonstrates asynchronous networking using [async_context](https://www.raspberrypi.com/documentation/pico-sdk/high_level.html#pico_async_context)
    on an [ESP-Hosted-FG firmware](https://github.com/Networking-for-Arduino/ESPHost).
    The program periodically requests a quote from the QOTD server, sends it to an echo server and prints response in reverse order.
*/

#ifndef ESPHOSTSPI
#error This example requires an ESP-Hosted-FG WiFi chip to be defined, see the documentation
// For example, add this to your boards.local.txt:
// rpipico.build.extra_flags=-DESPHOST_RESET=D5 -DESPHOST_HANDSHAKE=D7 -DESPHOST_DATA_READY=D6 -DESPHOST_CS=D1 -DESPHOSTSPI=SPI
#endif

#include "secrets.h"
#include <WiFi.h>
#include "AsyncTcpClient.hpp" // Include the new class for asynchronous TCP client
#include "ContextManager.hpp"
#include <iostream>
#include <algorithm>
#include "WorkerData.hpp" // To include the WorkerData definition
#include "ReceiveCallbackHandler.hpp"
// WiFi credentials
const auto *ssid = STASSID;
const auto *password = STAPSK;
// WiFi management
WiFiMulti multi;

// Server details
const auto *host = QOTD_HOST;
const auto *echo_host = ECHO_HOST;
constexpr  uint16_t port = 17; // QOTD service port
constexpr  uint16_t echo_port = 1235; // Echo service port

// TCP clients
AsyncTcp::AsyncTcpClient qotd_client;
AsyncTcp::AsyncTcpClient echo_client;

// Timing variables
unsigned long previous_qotd = 0;  // Last time QOTD was requested
unsigned long previous_echo = 0;  // Last time echo was sent
unsigned long previous_print = 0;  // Last time echo was sent

// Constants for intervals
constexpr  long interval = 5555;  // Interval for QOTD requests (milliseconds)
constexpr  long echo_interval = 3333;  // Interval for echo requests (milliseconds)
constexpr  long print_interval = 11111;

//  global asynchronous context managers and workers
auto ctx = std::make_shared<AsyncTcp::ContextManager>();
auto ctx1 = std::make_shared<AsyncTcp::ContextManager>();
auto print_worker = std::make_shared<AsyncTcp::Worker>();

// Buffer for storing the quote
auto tx_buffer = std::make_shared<std::string>();

constexpr int MAX_QOTD_SIZE = 512;
/**
 * @brief Connects to the "quote of the day" server and initiates a connection.
 *
 * This function resolves the host name to an IP address and attempts to connect
 * to the server on the specified port.
 */
void get_quote_of_the_day();

/**
 * @brief Connects to the echo server and sends data if available.
 *
 * This function checks if the echo client is connected. If not, it attempts to connect.
 * If connected and there is data in the transmission buffer, it sends the data to the server.
 */
void get_echo();

/**
 * @note Important: These callback functions MUST be declared with 'extern "C"'
 * because they are used as function pointers in the Pico SDK's C-based
 * async_when_pending_worker_t structure. Specifically, they must match the
 * do_work function pointer signature:
 *
 * void (*do_work)(async_context_t *context, async_when_pending_worker_t *worker);
 *
 * Without extern "C", C++ name mangling would make these functions incompatible
 * with the C-style function pointer expected by the SDK.
 */
/**
 * @brief Reads data from the QOTD server.
 *
 * @param context The asynchronous context.
 * @param worker The worker handling the read operation.
 */
extern "C" {
    void read_qotd(async_context_t *context, async_when_pending_worker_t *worker);
}
/**
 * @brief Reads data from the echo server.
 *
 * @param context The asynchronous context.
 * @param worker The worker handling the read operation.
 */
extern "C" {
    void read_echo(async_context_t *context, async_when_pending_worker_t *worker);
}

extern "C" {
    void print_out(async_context_t *context, async_when_pending_worker_t *worker);
}

void get_quote_of_the_day() {
    IPAddress remote_address;
    hostByName(host, remote_address, 1000);

    if (0 == qotd_client.connect(remote_address, port)) {
        DEBUGV("Failed to connect to QOTD server..\n");
    }
}

void get_echo() {
    if (!echo_client.connected()) {
        IPAddress remote_address;
        hostByName(echo_host, remote_address, 1000);

        if (0 == echo_client.connect(remote_address, echo_port)) {
            DEBUGV("Failed to connect to echo server..\n");
        }
    } else {
        ctx->acquireLock();
        if (!tx_buffer->empty()) {
            echo_client.write(tx_buffer->c_str(), tx_buffer->size());
        } else {
            DEBUGV("Nothing to send to echo server.\n");
        }
        ctx->releaseLock();
    }
}

void read_qotd(async_context_t *context, async_when_pending_worker_t *worker) {
    (void) context;
    auto *pData = static_cast<AsyncTcp::WorkerData *>(worker->user_data);

    // AsyncTcpClient pData->client
    if (pData && pData->client) {

        // Use the minimum of `read_size` and the QOTD protocol maximum size
        size_t safe_size = std::min(*pData->read_size, MAX_QOTD_SIZE);
        char buffer[safe_size];

        const size_t count = pData->client->read(buffer, *pData->read_size);
        Serial.println(buffer);
        /*
         Update the tx_buffer with the new quote
         A lock should not be acquired here.
         read_qotd is invoked as part of the async_when_pending_worker_t mechanism,
         and the async_context already ensures that the worker is executed under a lock
         */
        tx_buffer->assign(buffer, count);  // Store the quote in tx_buffer
    } else {
        DEBUGV("Invalid read size pointer\n");
    }

    worker->user_data = nullptr;
    worker->work_pending = false;
}

void print_out(async_context_t *context, async_when_pending_worker_t *worker) {
    (void) context;
    auto *pData = static_cast<AsyncTcp::WorkerData *>(worker->user_data);

    if (pData && pData->message) {
        Serial.println(pData->message->c_str());
    } else {
        DEBUGV("No message in the user data.\n");
    }

    worker->user_data = nullptr;
    worker->work_pending = false;
}

void read_echo(async_context_t *context, async_when_pending_worker_t *worker) {
    (void) context;
    auto *pData = static_cast<AsyncTcp::WorkerData *>(worker->user_data);

    if (pData && pData->client) {
        // Use the minimum of `read_size` and the QOTD protocol maximum size
        const size_t safe_size = std::min(*pData->read_size, MAX_QOTD_SIZE);
        char buffer[safe_size];
        const size_t count = pData->client->read(buffer, safe_size);
        (void) count;
        std::string reversed_quote = buffer;
        std::reverse(reversed_quote.begin(), reversed_quote.end());
        Serial.println(reversed_quote.c_str());
    } else {
        DEBUGV("No TCP client.\n");
    }

    worker->user_data = nullptr;
    worker->work_pending = false;
}

void print_heap_stats() {
    // Get heap data
    // 1. Get heap stats
    const int freeHeap = rp2040.getFreeHeap();
    const int usedHeap = rp2040.getUsedHeap();
    const int totalHeap = rp2040.getTotalHeap();

    // 2. Format the string with stats
    char print_me[64];
    auto print_me_length = std::make_unique<int>(
            snprintf(print_me, sizeof(print_me), "Free: %d, Used: %d, Total: %d", freeHeap, usedHeap, totalHeap)
    );

    auto data = std::make_unique<AsyncTcp::WorkerData>();  // Allocate WorkerData

    // Capture message
    auto message = std::make_unique<std::string>(print_me);

    data->read_size = std::move(print_me_length);       // Set read size in WorkerData
    data->message = std::move(message);       // Set message in WorkerData

    // Pass WorkerData to the worker and notify context of pending work
    print_worker->setWorkerData(std::move(data));
    DEBUGV("\nSet print worker to pending.\n");
    DEBUGV("Core %d\n", ctx1->getCore());
    ctx1->setWorkPending(*print_worker);
}

/**
 * @brief Initializes the Wi-Fi connection and asynchronous context.
 *
 * This function sets up the serial communication, connects to the WiFi network,
 * and initializes the asynchronous context for handling TCP connections.
 */
[[maybe_unused]] void setup() {
    Serial.begin(115200);
    while(!Serial)
    {
        delay(10);
    };
    delay(5000);
    DEBUGV("C0: Blue leader standing by...\n");
    RP2040::enableDoubleResetBootloader();

    // We start by connecting to a Wi-Fi network
    DEBUGV("Connecting to %s\n", ssid);

    multi.addAP(ssid, password);

    if (multi.run() != WL_CONNECTED) {
        DEBUGV("Unable to connect to network, rebooting in 10 seconds...\n");
        delay(10000);
        rp2040.reboot();
    }

    Serial.println("WiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    // DEBUGV("IP address:  %s\n", ip);

    if (!ctx->initDefaultContext()) {
        // Error handling can be performed here if context initialization fails
        DEBUGV("ctx init failed on the Core 0\n");
    }

    DEBUGV("Core %d\n", ctx->getCore());

    const auto echo_worker = std::make_shared<AsyncTcp::Worker>();

    echo_worker->setWorkFunction(read_echo);

    if(!ctx->addWorker(*echo_worker)) {
        DEBUGV("Failed to add echo worker\n");
    }

    std::shared_ptr<AsyncTcp::ReceiveCallbackHandler> echo_handler = AsyncTcp::EventHandler::create<AsyncTcp::ReceiveCallbackHandler>(ctx, echo_worker);

    echo_client.setOnReceiveCallback(std::move(echo_handler));

    const auto qotd_worker = std::make_shared<AsyncTcp::Worker>();

    qotd_worker->setWorkFunction(read_qotd);

    if(!ctx->addWorker(*qotd_worker)) {
        DEBUGV("Failed to add qotd worker\n");
    }

    std::shared_ptr<AsyncTcp::ReceiveCallbackHandler> qotd_handler = AsyncTcp::EventHandler::create<AsyncTcp::ReceiveCallbackHandler>(ctx, qotd_worker);

    qotd_client.setOnReceiveCallback(std::move(qotd_handler));
}

// Running on core1
[[maybe_unused]] void setup1() {
    delay(5000);
    DEBUGV("C1: Red leader standing by...\n");

    if (!ctx1->initDefaultContext()) {
        // Error handling can be performed here if context initialization fails
        DEBUGV("CTX init failed on Core 1\n");
    }

    DEBUGV("Core %d\n", ctx1->getCore());
    print_worker->setWorkFunction(print_out);

    if(!ctx1->addWorker(*print_worker)) {
        DEBUGV("Failed to add print worker\n");
    }
}

/**
 * @brief Main loop function.
 *
 * This function is called repeatedly and handles the periodic requests to the QOTD and echo servers.
 * It checks the elapsed time since the last request and calls the appropriate functions if the interval has passed.
 */
[[maybe_unused]] void loop() {
    const unsigned long currentMillis = millis();
    if (currentMillis - previous_qotd >= interval) {
        previous_qotd = currentMillis;
        get_quote_of_the_day();
        DEBUGV("Core in quote: %d\n", ::ctx1->getCore());
    }

    if (currentMillis - previous_echo >= echo_interval) {
        previous_echo = currentMillis;
        get_echo();
    }

    if (currentMillis - previous_print >= print_interval) {
        previous_print = currentMillis;
        print_heap_stats();
    }
}

[[maybe_unused]] void loop1() {
}
