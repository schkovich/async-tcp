/**
 * @file IoWrite.cpp
 * @brief Implementation of thread-safe wrapper for AsyncTcpClient write operations
 *
 * This file implements the IoWrite class which provides thread-safe access to
 * AsyncTcpClient write operations using the SyncBridge pattern.
 *
 * @author Goran
 * @date 2025-02-27
 * @ingroup AsyncTCPClient
 */

#include "e5/IoWrite.hpp"
#include <cstring>

namespace e5 {

    /**
     * @brief Constructs an IoWrite with the specified context manager and client
     *
     * @param ctx Shared context manager for synchronized execution
     * @param client Reference to the AsyncTcpClient
     */
    IoWrite::IoWrite(const ContextManagerPtr& ctx, AsyncTcpClient& client)
        : SyncBridge(ctx), m_client(client) {}

    /**
     * @brief Executes write operations in a thread-safe manner
     *
     * This method is called by the SyncBridge to perform write operations.
     * It ensures that all operations happen on the core where the ContextManager
     * was initialized, providing thread safety.
     *
     * @param payload Write operation instruction
     * @return PICO_OK on success
     */
    uint32_t IoWrite::onExecute(const SyncPayloadPtr payload) {

        switch (auto* write_payload = static_cast<WritePayload*>(payload.get()); write_payload->type) {
            case WritePayload::BUFFER:
                write_payload->result = m_client.write(write_payload->data, write_payload->size);
                break;

            case WritePayload::STRING:
                write_payload->result = m_client.write(
                    reinterpret_cast<const uint8_t*>(write_payload->str),
                    strlen(write_payload->str)
                );
                break;

            case WritePayload::STREAM:
                write_payload->result = m_client.write(*write_payload->stream);
                break;
        }

        return PICO_OK;
    }

    /**
     * @brief Writes data from a buffer to the client
     *
     * This method writes data from the provided buffer to the AsyncTcpClient.
     * Thread-safe through SyncBridge integration, can be called from any core
     * or interrupt context.
     *
     * @param data Pointer to the data buffer
     * @param size Size of the data buffer
     * @return Number of bytes written
     */
    size_t IoWrite::write(const uint8_t* data, const size_t size) {
        auto payload = std::make_unique<WritePayload>();
        payload->type = WritePayload::BUFFER;
        payload->data = data;
        payload->size = size;
        execute(std::move(payload));
        return payload->result;
    }

    /**
     * @brief Writes a single byte to the client
     *
     * This method writes a single byte to the AsyncTcpClient.
     * Thread-safe through SyncBridge integration, can be called from any core
     * or interrupt context.
     *
     * @param b Byte to write
     * @return Number of bytes written (0 or 1)
     */
    size_t IoWrite::write(const uint8_t b) {
        return write(&b, 1);
    }

    /**
     * @brief Writes a null-terminated string to the client
     *
     * This method writes a null-terminated string to the AsyncTcpClient.
     * Thread-safe through SyncBridge integration, can be called from any core
     * or interrupt context.
     *
     * @param str Null-terminated string to write
     * @return Number of bytes written
     */
    size_t IoWrite::write(const char* str) {
        auto payload = std::make_unique<WritePayload>();
        payload->type = WritePayload::STRING;
        payload->str = str;
        execute(std::move(payload));
        return payload->result;
    }

    /**
     * @brief Writes data from a stream to the client
     *
     * This method writes all available data from the provided stream to the AsyncTcpClient.
     * Thread-safe through SyncBridge integration, can be called from any core
     * or interrupt context.
     *
     * @param stream Stream to read data from
     * @return Number of bytes written
     */
    size_t IoWrite::write(Stream& stream) {
        auto payload = std::make_unique<WritePayload>();
        payload->type = WritePayload::STREAM;
        payload->stream = &stream;
        execute(std::move(payload));
        return payload->result;
    }

} // namespace e5