/**
 * @file TcpClient.hpp
 * @brief Header file for the AsyncTcpClient class for Arduino Nano RP2040
 * Connect.
 *
 * This library provides asynchronous TCP client functionalities with support
 * for the ESP32 co-processor. The library is a refactored version of the
 * WiFiClient library.
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
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 2.1 of the License, or (at your option)
 * any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
 * details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA.
 */
#pragma once

#include "WiFi.h"

#include "IoRxBuffer.hpp"
#include "PerpetualBridge.hpp"
#include "TcpWriter.hpp"
#include "TcpClientSyncAccessor.hpp"
#include <memory>

namespace async_tcp {

#ifndef TCP_MSS
#define TCP_MSS 1460 // lwip1.4
#endif

#define ASYNC_TCP_CLIENT_MAX_PACKET_SIZE TCP_MSS
#define ASYNC_TCP_CLIENT_MAX_FLUSH_WAIT_MS 300

#define TCP_DEFAULT_KEEP_ALIVE_IDLE_SEC 7200   // 2 hours
#define TCP_DEFAULT_KEEP_ALIVE_INTERVAL_SEC 75 // 75 sec
#define TCP_DEFAULT_KEEP_ALIVE_COUNT 9         // fault after 9 failures

    class TcpClientContext;

    class WiFiServer; // Is it necessary and used?

    using namespace std::placeholders;

    //  Local alias for arduino::IPAddress
    using AIPAddress = IPAddress;
    //  Local alias for arduino::String
    using AString = String;

    using TcpWriterPtr = std::unique_ptr<TcpWriter>;
    using TcpClientSyncAccessorPtr = std::unique_ptr<TcpClientSyncAccessor>;

    /**
     * @class TcpClient
     * @brief Asynchronous TCP client class.
     *
     * This class implements a TCP client that supports asynchronous operations.
     * It derives from the Arduino Client and SList for managing client lists.
     */
    class TcpClient {

        protected:
            /**
             * @brief A protected constructor to initialize the AsyncTcpClient
             * with a specific context.
             * @param ctx Pointer to an AsyncTcpClientContext object.
             */
            explicit TcpClient(TcpClientContext *ctx);

        public:
            /**
             * @brief Default constructor for the AsyncTcpClient class.
             */
            TcpClient();

            /**
             * @brief Destructor for the AsyncTcpClient class.
             */
            virtual ~TcpClient();

            virtual uint8_t status();

            /**
             * @brief Establish an asynchronous connection to a remote host.
             *
             * This method initiates a TCP connection to the specified IP
             * address and port. It manages the internal context (`_ctx`) and
             * ensures proper resource handling by using reference counting
             * through `ref()` and `unref()`. If a connection already exists, it
             * is stopped, and the current context is de-referenced and released
             * to avoid two clients sharing the same context and PCB.
             *
             * Steps:
             * 1. If a previous connection exists (`_ctx` is not null), it is
             * stopped and cleaned up by calling `stop()`, de-referencing the
             * context (`_ctx->unref()`), and setting `_ctx` to `nullptr`.
             * 2. A new TCP control block (PCB) is created using `tcp_new()`. If
             * no PCB can be allocated, the method returns 0, indicating
             * failure.
             * 3. If the client has a local port (`_localPort > 0`), the local
             * port of the PCB is set, and
             *    `_localPort` is incremented for future connections.
             * 4. A new `AsyncTcpClientContext` object is created, and `_ctx` is
             * assigned this context. The context is referenced using `ref()`,
             * ensuring it is tracked for proper resource management.
             * 5. Various callback functions are set on `_ctx`, including
             * connection, error, receive, and acknowledgment handlers, ensuring
             * proper handling of TCP events.
             * 6. The connection attempt to the specified IP and port is
             * initiated through `_ctx->connect()`. If the connection fails
             * (`res == 0`), the context is cleaned up with `unref()` and `_ctx`
             * is reset to `nullptr`, returning 0 to indicate failure.
             * 7. If the connection succeeds, default TCP settings (`Sync` and
             * `NoDelay`) are applied, and the method returns 1.
             *
             * @param ip The IP address of the remote host. Uses the alias
             * AIPAddress, which maps to `arduino::IPAddress`.
             * @param port The port number of the remote host.
             * @return int Returns 1 on successful connection, 0 on failure.
             *
             * @note The `ref()` and `unref()` methods are used to manage the
             * reference count of `_ctx`. If the context is no longer needed
             * (e.g., connection failure), `unref()` is called to decrement the
             * reference count and, if the count reaches zero, the context is
             * cleaned up and deleted.
             *
             * @warning This method ensures that no two clients share the same
             * PCB or context. By cleaning up the previous context before
             * creating a new one, resource leaks and unintended behavior from
             * shared state are avoided.
             */
            int connect(AIPAddress ip, uint16_t port);

            /**
             * @brief Establish an asynchronous connection to a remote host
             * using a hostname.
             *
             * This method resolves the hostname to an IP address and initiates
             * a TCP connection to the specified port. It uses the internal
             * `hostByName` function (from `WiFiClient`) to perform the DNS
             * lookup. If the hostname is resolved successfully, the method
             * forwards the connection request to the IP-based `connect()`
             * method, which handles resource and context management.
             *
             * @param host The hostname of the remote server (as a `const
             * char*`).
             * @param port The port number of the remote host.
             * @return int Returns 1 on successful connection, or 0 if the
             * hostname could not be resolved or the connection fails.
             *
             * @note This method calls the IP-based `connect()` internally after
             * resolving the hostname. All resource management, such as
             * reference counting of `_ctx` using `ref()` and `unref()`, is
             * handled by the IP-based `connect()` method.
             */
            int connect(const char *host, uint16_t port);

            /**
             * @brief Establish an asynchronous connection to a remote host
             * using a hostname.
             *
             * This method resolves the hostname to an IP address and initiates
             * a TCP connection to the specified port. It uses the `hostByName`
             * function from `LwipEthernet` to perform the DNS lookup. If the
             * hostname is resolved successfully, the method forwards the
             * connection request to the IP-based `connect()` method, which
             * handles resource and context management.
             *
             * @param host The hostname of the remote server (as a `AString`).
             * @param port The port number of the remote host.
             * @return int Returns 1 on successful connection, or 0 if the
             * hostname could not be resolved or the connection fails.
             *
             * @note This method calls the IP-based `connect()` internally after
             * resolving the hostname. All resource management, such as
             * reference counting of `_ctx` using `ref()` and `unref()`, is
             * handled by the IP-based `connect()` method.
             */
            virtual int connect(const AString &host, uint16_t port);

            [[nodiscard]] size_t write(uint8_t b) const;

            size_t write(const uint8_t *buf, size_t size) const;

            /**
             * @brief Write a single chunk directly to TCP connection.
             *
             * This method bypasses the blocking _write_from_source and _write_some methods,
             * directly calling the new TcpClientContext::writeChunk() method for
             * worker-based write operations.
             *
             * @param data Pointer to binary data to write
             * @param size Size of data chunk
             */
            void writeChunk(const uint8_t* data, size_t size) const;

            void stop() const {
                if (const auto err = stop(0); err == false) {
                    DEBUGWIRE("[:i%d] :stop timeout\n", getClientId());
                }
            }

            [[nodiscard]] bool stop(unsigned int maxWaitMs) const;

            /**
             * @brief Properly shutdown the connection and clean up resources.
             *
             * This method calls stop() to close the connection and then sets
             * both _ctx and _rx to nullptr, ensuring the TcpClient is in a
             * clean state for subsequent connect() calls.
             *
             * @param maxWaitMs Maximum time to wait for connection closure
             * @return true if shutdown was successful, false otherwise
             */
            bool shutdown(unsigned int maxWaitMs = 0);

            [[maybe_unused]] [[nodiscard]] AIPAddress remoteIP() const;

            [[maybe_unused]] [[nodiscard]] uint16_t remotePort() const;

            [[nodiscard]] AIPAddress localIP() const;

            [[maybe_unused]] [[nodiscard]] uint16_t localPort() const;

            [[maybe_unused]] static void
            setLocalPortStart(const uint16_t port) {
                _localPort = port;
            }

            friend class TcpClientSyncAccessor;

            void
            keepAlive(uint16_t idle_sec = TCP_DEFAULT_KEEP_ALIVE_IDLE_SEC,
                      uint16_t intv_sec = TCP_DEFAULT_KEEP_ALIVE_INTERVAL_SEC,
                      uint8_t count = TCP_DEFAULT_KEEP_ALIVE_COUNT) const;

            [[maybe_unused]] [[nodiscard]] bool isKeepAliveEnabled() const;

            [[maybe_unused]] [[nodiscard]] uint16_t getKeepAliveIdle() const;

            [[maybe_unused]] [[nodiscard]] uint16_t
            getKeepAliveInterval() const;

            [[maybe_unused]] [[nodiscard]] uint8_t getKeepAliveCount() const;

            [[maybe_unused]] void disableKeepAlive() const {
                keepAlive(0, 0, 0);
            }

            // default NoDelay=False (Nagle=True=!NoDelay)
            // Nagle is for shortly delaying outgoing data, to send less/bigger packets
            // Nagle should be disabled for telnet-like/interactive streams
            [[maybe_unused]] static void setDefaultNoDelay(bool noDelay);

            [[maybe_unused]] static bool getDefaultNoDelay();

            [[maybe_unused]] [[nodiscard]] bool getNoDelay() const;

            void setNoDelay(bool no_delay) const;

            void setOnReceivedCallback(std::unique_ptr<PerpetualBridge> bridge);
            void setOnConnectedCallback(PerpetualBridgePtr bridge);
            void setOnFinCallback(std::unique_ptr<PerpetualBridge> bridge);
            void setOnWriterErrorCallback(PerpetualBridgePtr bridge);

            /**
             * @brief Set the TcpWriter for chunked write operations
             *
             * When a writer is set, the write() method will delegate to the writer
             * for efficient chunking and flow control instead of using the basic
             * synchronous write implementation. Takes ownership of the writer.
             *
             * @param writer Unique pointer to TcpWriter instance
             */
            void setWriter(TcpWriterPtr writer);

            /**
             * @brief Set the client ID for this TcpClient instance.
             * @param id The client ID to assign (uint8_t)
             */
            void setClientId(const uint8_t id) { m_client_id = id; }

            /**
             * @brief Set the sync accessor for this TcpClient instance.
             * @param accessor Unique pointer to TcpClientSyncAccessor instance
             */
            void setSyncAccessor(TcpClientSyncAccessorPtr accessor);

            // Method needed for the "jump" pattern in static callbacks
            [[nodiscard]] TcpClientContext *getContext() const {
                return _ctx;
            }

            [[nodiscard]] TcpWriter *getWriter() const {
                return m_writer.get();
            }

            [[nodiscard]] TcpClientSyncAccessor *getSyncAccessor() const {
                return m_sync_accessor.get();
            }

            /**
             * @brief Get the client ID (for internal logging)
             * @return uint8_t client id
             */
            [[nodiscard]] uint8_t getClientId() const { return m_client_id; }

        protected:
            PerpetualBridgePtr _received_callback_bridge;
            PerpetualBridgePtr _connected_callback_bridge;
            PerpetualBridgePtr _fin_callback_bridge;
            PerpetualBridgePtr _writer_error_callback_bridge;

            TcpClientContext *_ctx;

            static uint16_t _localPort;

            TcpWriterPtr m_writer{};  ///< Writer for chunked operations
            TcpClientSyncAccessorPtr m_sync_accessor; ///< Sync accessor for thread-safe operations

            // --- Client ID for logging and traceability ---
            uint8_t m_client_id = 0; // Smallest integer type for client id

            void _onConnectCallback() const;

            void _onFinCallback() const;

            void _onErrorCallback(err_t err) const;

            void _onReceiveCallback() const;

            void _onAckCallback(const tcp_pcb *tpcb, uint16_t len) const;

            void checkAndHandleWriteTimeout() const;
        private:
            unsigned long _timeout;      // number of milliseconds to wait for the next char before aborting timed read

            virtual uint8_t _ts_status();
            // Thread-context correct connect implementation (must be called under async-context lock on networking core)
            int _ts_connect(AIPAddress ip, uint16_t port);
    };
} // namespace AsyncTcp
