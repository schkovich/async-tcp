/**
 * @file IoWrite.hpp
 * @brief Thread-safe wrapper for AsyncTcpClient write operations
 *
 * This file defines the IoWrite class which provides thread-safe access to
 * AsyncTcpClient write operations using the SyncBridge pattern. It ensures that
 * all write operations happen on the core where the ContextManager was initialized,
 * providing proper thread safety without modifying the AsyncTcpClient class.
 *
 * @author Goran
 * @date 2025-02-27
 * @ingroup AsyncTCPClient
 */

#pragma once
#include "AsyncTcpClient.hpp"
#include "ContextManager.hpp"
#include "SyncBridge.hpp"

namespace e5 {

    using namespace AsyncTcp;

    /**
     * @class IoWrite
     * @brief Thread-safe wrapper for AsyncTcpClient write operations
     *
     * This class extends SyncBridge to provide thread-safe access to AsyncTcpClient
     * write operations. It ensures that all write operations happen on the core where
     * the ContextManager was initialised, providing proper thread safety without
     * modifying the AsyncTcpClient class.
     *
     * Usage example:
     * ```cpp
     * IoWrite io_write(ctx, client);
     * io_write.write("Hello, World!", 13);
     * ```
     */
    class IoWrite final : public SyncBridge {
    private:
        AsyncTcpClient& m_client;  ///< Reference to the AsyncTcpClient

        /**
         * @struct WritePayload
         * @brief Payload for write operations
         *
         * This structure carries the data and parameters for write operations.
         * It is used with the SyncBridge pattern to ensure thread-safe access.
         */
        struct WritePayload final : SyncPayload {
            enum WriteType {
                BUFFER,     ///< Write from a buffer
                STRING,     ///< Write from a null-terminated string
                STREAM      ///< Write from a stream
            };

            WriteType type;             ///< Write operation type
            const uint8_t* data;        ///< Data buffer for BUFFER operations
            const char* str;            ///< String for STRING operations
            Stream* stream;             ///< Stream for STREAM operations
            size_t size;                ///< Size for BUFFER operations
            size_t result;              ///< Result of the write operation

            WritePayload() : type(BUFFER), data(nullptr), str(nullptr), stream(nullptr), size(0), result(0) {}
        };

        /**
         * @brief Executes write operations in a thread-safe manner
         *
         * This method is called by the SyncBridge to perform write operations.
         * It ensures that all operations happen on the core where the ContextManager
         * was initialised, providing thread safety.
         *
         * @param payload Write operation instruction
         * @return PICO_OK on success
         */
        uint32_t onExecute(SyncPayloadPtr payload) override;

    public:
        /**
         * @brief Constructs an IoWrite with the specified context manager and client
         *
         * @param ctx Shared context manager for synchronized execution
         * @param client Reference to the AsyncTcpClient
         */
        explicit IoWrite(const ContextManagerPtr& ctx, AsyncTcpClient& client);

        /**
         * @brief Writes data from a buffer to the client
         *
         * This method writes data from the provided buffer to the AsyncTcpClient.
         * Thread-safe through SyncBridge integration, can be called from any core
         * or interrupt context.
         *
         * @param data Pointer to the data buffer
         * @param size Size of the data buffer
         * @return Amount bytes written
         */
        size_t write(const uint8_t* data, size_t size);

        /**
         * @brief Writes a single byte to the client
         *
         * This method writes a single byte to the AsyncTcpClient.
         * Thread-safe through SyncBridge integration can be called from any core
         * or interrupt context.
         *
         * @param b Byte to write
         * @return Amount bytes written, 0 or 1.
         */
        size_t write(uint8_t b);

        /**
         * @brief Writes a null-terminated string to the client
         *
         * This method writes a null-terminated string to the AsyncTcpClient.
         * Thread-safe through SyncBridge integration, can be called from any core
         * or interrupt context.
         *
         * @param str Null-terminated string to write
         * @return Amount bytes written
         */
        size_t write(const char* str);

        /**
         * @brief Writes data from a stream to the client
         *
         * This method writes all available data from the provided stream to the AsyncTcpClient.
         * Thread-safe through SyncBridge integration, can be called from any core
         * or interrupt context.
         *
         * @param stream Stream to read data from
         * @return Amount bytes written
         */
        size_t write(Stream& stream);
    };

} // namespace e5