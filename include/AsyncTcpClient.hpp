/**
 * @file AsyncTcpClient.hpp
 * @brief Header file for the AsyncTcpClient class for Arduino Nano RP2040 Connect.
 *
 * This library provides asynchronous TCP client functionalities with support for
 * the ESP32 co-processor. The library is a refactored version of the WiFiClient library.
 *
 * @modified Ivan Grokhotkov
 * @brief Added esp8266 support
 * @date 2014
 * @modified Earle F. Philhower, III
 * @brief Hacked to tiny bits and set on fire. Added Pico W support.
 * @date 2022
 *
 * @copyright
 * Copyright (c) 2011-2014 Arduino. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or modify it under the terms
 * of the GNU Lesser General Public License as published by the Free Software Foundation;
 * either version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License along with this library;
 * if not, write to the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA.
 */
#pragma once

#include "WiFi.h"

#include <memory>
#include "Print.h"
#include "Client.h"
#include "IPAddress.h"
#include "EventHandler.hpp"

namespace AsyncTcp {

#ifndef TCP_MSS
#define TCP_MSS 1460 // lwip1.4
#endif

#define ASYNC_TCP_CLIENT_MAX_PACKET_SIZE TCP_MSS
#define ASYNC_TCP_CLIENT_MAX_FLUSH_WAIT_MS 300

#define TCP_DEFAULT_KEEP_ALIVE_IDLE_SEC          7200 // 2 hours
#define TCP_DEFAULT_KEEP_ALIVE_INTERVAL_SEC      75   // 75 sec
#define TCP_DEFAULT_KEEP_ALIVE_COUNT             9    // fault after 9 failures

    class AsyncTcpClientContext;

    class WiFiServer; // Is it needed and used?

    using namespace std::placeholders;

//  Local alias for arduino::IPAddress
    using AIPAddress = arduino::IPAddress;
//  Local alias for arduino::String
    using AString = arduino::String;

    /**
     * @class AsyncTcpClient
     * @brief Asynchronous TCP client class.
     *
     * This class implements a TCP client that supports asynchronous operations.
     * It derives from the Arduino Client and SList for managing client lists.
     */
    class   AsyncTcpClient : public arduino::Client, public SList<AsyncTcpClient> {

    protected:
        /**
         * @brief Protected constructor to initialize the AsyncTcpClient with a specific context.
         * @param ctx Pointer to an AsyncTcpClientContext object.
         */
        explicit AsyncTcpClient(AsyncTcpClientContext *ctx);

    public:
        /**
         * @brief Default constructor for the AsyncTcpClient class.
         */
        AsyncTcpClient();

        /**
         * @brief Destructor for the AsyncTcpClient class.
         */
        virtual ~AsyncTcpClient();

        /**
         * @brief Copy constructor for AsyncTcpClient.
         *
         * This constructor creates a new instance of `AsyncTcpClient` by copying the state of another `AsyncTcpClient` object.
         * It performs a shallow copy of the internal context (`_ctx`), increments the reference count for the context using `ref()`,
         * and adds the new object to the `SList`.
         *
         * @param other The `AsyncTcpClient` object to copy from.
         *
         * @note
         * - The internal context (`_ctx`) is shared between the two instances, and its reference count is incremented to ensure
         *   proper resource management.
         * - The `_timeout`, `_localPort`, and `_owned` members are copied from the source object (`other`).
         * - The new client is automatically added to the `SList` upon creation.
         */
        AsyncTcpClient(const AsyncTcpClient &);

        /**
         * @brief Assignment operator for AsyncTcpClient.
         *
         * Assigns the state of another AsyncTcpClient object to this one.
         * This operator manages the internal client context (_ctx), ensuring
         * proper reference counting by un-referencing the current context and
         * referencing the new context from the source object. It also copies other
         * relevant internal properties such as timeouts and ownership state.
         *
         * @param other The source AsyncTcpClient object to assign from.
         * @return AsyncTcpClient& A reference to the updated AsyncTcpClient object.
         *
         * @note This operator handles self-assignment by checking if the current
         * object is the same as the source object. In the case of self-assignment,
         * no changes are made to prevent unnecessary operations.
         *
         * @warning Proper memory management is ensured by calling `_ctx->unref()`
         * for the current context (if it exists), ensuring proper resource
         * cleanup when the reference count reaches zero. before assigning the new context,
         * `_ctx->ref()` is called after assignment to update reference counting.
         */
         AsyncTcpClient &operator=(const AsyncTcpClient &);

        /**
         * @brief Creates a copy of the current AsyncTcpClient object.
         *
         * This method creates a new instance of `AsyncTcpClient` by copying the current object.
         * It returns a unique pointer to the newly created instance. The method is virtual to
         * support polymorphic behavior and ensure that derived classes can implement their own
         * `clone()` method to prevent object slicing.
         *
         * @return std::unique_ptr<AsyncTcpClient> A unique pointer to the copied `AsyncTcpClient` object.
         *
         * @warning This method, as currently implemented, may cause object slicing if used on a derived class.
         * The derived class should override the `clone()` method to return a unique pointer to its own type.
         *
         * @see The C++ Core Guidelines on copying virtual classes:
         * https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#Rc-copy-virtual
         */
        [[maybe_unused]] [[nodiscard]] virtual std::unique_ptr<AsyncTcpClient> clone() const;

        virtual uint8_t status();

        /**
         * @brief Establish an asynchronous connection to a remote host.
         *
         * This method initiates a TCP connection to the specified IP address and port. It manages
         * the internal context (`_ctx`) and ensures proper resource handling by using reference counting
         * through `ref()` and `unref()`. If a connection already exists, it is stopped, and the current
         * context is de-referenced and released to avoid two clients sharing the same context and PCB.
         *
         * Steps:
         * 1. If a previous connection exists (`_ctx` is not null), it is stopped and cleaned up by calling
         *    `stop()`, de-referencing the context (`_ctx->unref()`), and setting `_ctx` to `nullptr`.
         * 2. A new TCP control block (PCB) is created using `tcp_new()`. If no PCB can be allocated, the
         *    method returns 0, indicating failure.
         * 3. If the client has a local port (`_localPort > 0`), the local port of the PCB is set, and
         *    `_localPort` is incremented for future connections.
         * 4. A new `AsyncTcpClientContext` object is created, and `_ctx` is assigned this context.
         *    The context is referenced using `ref()`, ensuring it is tracked for proper resource management.
         * 5. Various callback functions are set on `_ctx`, including connection, error, receive, and acknowledgment
         *    handlers, ensuring proper handling of TCP events.
         * 6. The connection attempt to the specified IP and port is initiated through `_ctx->connect()`.
         *    If the connection fails (`res == 0`), the context is cleaned up with `unref()` and `_ctx` is reset
         *    to `nullptr`, returning 0 to indicate failure.
         * 7. If the connection succeeds, default TCP settings (`Sync` and `NoDelay`) are applied, and the method returns 1.
         *
         * @param ip The IP address of the remote host. Uses the alias AIPAddress, which maps to `arduino::IPAddress`.
         * @param port The port number of the remote host.
         * @return int Returns 1 on successful connection, 0 on failure.
         *
         * @note The `ref()` and `unref()` methods are used to manage the reference count of `_ctx`. If the context
         * is no longer needed (e.g., connection failure), `unref()` is called to decrement the reference count and,
         * if the count reaches zero, the context is cleaned up and deleted.
         *
         * @warning This method ensures that no two clients share the same PCB or context. By cleaning up the previous
         * context before creating a new one, resource leaks and unintended behavior from shared state are avoided.
         */
        int connect(AIPAddress ip, uint16_t port) override;

        /**
         * @brief Establish an asynchronous connection to a remote host using a hostname.
         *
         * This method resolves the hostname to an IP address and initiates a TCP connection to
         * the specified port. It uses the internal `hostByName` function (from `WiFiClient`) to
         * perform the DNS lookup. If the hostname is resolved successfully, the method forwards
         * the connection request to the IP-based `connect()` method, which handles resource and
         * context management.
         *
         * @param host The hostname of the remote server (as a `const char*`).
         * @param port The port number of the remote host.
         * @return int Returns 1 on successful connection, or 0 if the hostname could not be resolved or the connection fails.
         *
         * @note This method calls the IP-based `connect()` internally after resolving the hostname. All resource
         * management, such as reference counting of `_ctx` using `ref()` and `unref()`, is handled by the IP-based `connect()` method.
         */
        int connect(const char *host, uint16_t port) override;

        /**
         * @brief Establish an asynchronous connection to a remote host using a hostname.
         *
         * This method resolves the hostname to an IP address and initiates a TCP connection to
         * the specified port. It uses the `hostByName` function from `LwipEthernet` to perform
         * the DNS lookup. If the hostname is resolved successfully, the method forwards
         * the connection request to the IP-based `connect()` method, which handles resource and
         * context management.
         *
         * @param host The hostname of the remote server (as a `AString`).
         * @param port The port number of the remote host.
         * @return int Returns 1 on successful connection, or 0 if the hostname could not be resolved or the connection fails.
         *
         * @note This method calls the IP-based `connect()` internally after resolving the hostname. All resource
         * management, such as reference counting of `_ctx` using `ref()` and `unref()`, is handled by the IP-based `connect()` method.
         */
        virtual int connect(const AString &host, uint16_t port);

        size_t write(uint8_t b) override;

        size_t write(const uint8_t *buf, size_t size) override;

        size_t write(Stream &stream);

        int available() override;

        int read() override;

        int read(uint8_t *buf, size_t size) override;

        int read(char *buf, size_t size);

        int peek() override;

        virtual size_t peekBytes(uint8_t *buffer, size_t length);

        [[maybe_unused]] size_t peekBytes(char *buffer, const size_t length) {
          return peekBytes(reinterpret_cast<uint8_t *>(buffer), length);
        }

        [[nodiscard]] const char* peekBuffer() const;

        [[nodiscard]] size_t peekAvailable() const;

        void peekConsume(size_t size) const;

        void flush() override {
            (void) flush(
                    0);    // wait for all outgoing characters to be sent, output buffer should be empty after this call
        }

        void stop() override {
            (void) stop(0);
        }

        bool flush(unsigned int maxWaitMs);

        bool stop(unsigned int maxWaitMs);

        uint8_t connected() override;

        explicit operator bool() override;

        [[maybe_unused]] AIPAddress remoteIP() const;

        [[maybe_unused]] uint16_t remotePort() const;

        AIPAddress localIP();

        [[maybe_unused]] uint16_t localPort();

        [[maybe_unused]] static void setLocalPortStart(uint16_t port) {
            _localPort = port;
        }

        int availableForWrite() override;

        friend class WiFiServer;

        using Print::write;

        [[maybe_unused]] static void stopAll();

        [[maybe_unused]] static void stopAllExcept(AsyncTcpClient *c);

        void keepAlive(uint16_t idle_sec = TCP_DEFAULT_KEEP_ALIVE_IDLE_SEC,
                       uint16_t intv_sec = TCP_DEFAULT_KEEP_ALIVE_INTERVAL_SEC,
                       uint8_t count = TCP_DEFAULT_KEEP_ALIVE_COUNT);

        [[maybe_unused]] [[nodiscard]] bool isKeepAliveEnabled() const;

        [[maybe_unused]] [[nodiscard]] uint16_t getKeepAliveIdle() const;

        [[maybe_unused]] [[nodiscard]] uint16_t getKeepAliveInterval() const;

        [[maybe_unused]] [[nodiscard]] uint8_t getKeepAliveCount() const;

        [[maybe_unused]] void disableKeepAlive() {
            keepAlive(0, 0, 0);
        }

        // default NoDelay=False (Nagle=True=!NoDelay)
        // Nagle is for shortly delaying outgoing data, to send less/bigger packets
        // Nagle should be disabled for telnet-like/interactive streams
        // Nagle is meaningless/ignored when Sync=true
        [[maybe_unused]] static void setDefaultNoDelay(bool noDelay);

        [[maybe_unused]] static bool getDefaultNoDelay();

        [[maybe_unused]] [[nodiscard]] bool getNoDelay() const;

        void setNoDelay(bool no_delay) const;

        // default Sync=false
        // When sync is true, all writes are automatically flushed.
        // This is slower but also does not allocate
        // temporary memory for sending data
        [[maybe_unused]] static void setDefaultSync(bool sync);

        [[maybe_unused]] static bool getDefaultSync();

        [[maybe_unused]] [[nodiscard]] bool getSync() const;

        void setSync(bool sync) const;

        void setOnReceiveCallback(std::shared_ptr<EventHandler> handler);
        void setOnConnectedCallback(std::shared_ptr<EventHandler> handler);

    protected:

        std::shared_ptr<EventHandler> _receive_callback_handler;
        std::shared_ptr<EventHandler> _connected_callback_handler;

        [[maybe_unused]] static int8_t _s_connected(void *arg, void *tpcb, int8_t err);

        [[maybe_unused]] static void _s_err(void *arg, int8_t err);

        static int8_t _connected(void *tpcb, int8_t err);

        [[maybe_unused]] void _err(int8_t err);

        inline static bool defaultSync = true;

        AsyncTcpClientContext *_ctx;

        AsyncTcpClient *_owned;

        static uint16_t _localPort;

        std::function<void(std::unique_ptr<int>)> _receiveCallback;

        void _onConnectCallback() const;

        void _onErrorCallback(err_t err);

        void _onReceiveCallback(std::unique_ptr<int> size) const;

        void _onAckCallback(struct tcp_pcb *tpcb, uint16_t len) const;
    };
} // namespace AsyncTcp