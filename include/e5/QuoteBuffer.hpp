/**
 * @file QuoteBuffer.hpp
 * @brief Thread-safe buffer for storing and accessing the quote of the day
 *
 * This file defines the QuoteBuffer class which provides thread-safe access to
 * a string buffer using the SyncBridge pattern.
 *
 * @author Goran
 * @date 2025-02-20
 * @ingroup AsyncTCPClient
 *
 */
#pragma once
#include <atomic>
#include <string>
#include "ContextManager.hpp"
#include "SyncBridge.hpp"

namespace e5 {

using namespace AsyncTcp;

/**
 * @class QuoteBuffer
 * @brief Thread-safe buffer for storing and accessing string data
 *
 * This class extends SyncBridge to provide thread-safe access to a string buffer.
 * It ensures that all modifications to the buffer happen on the core where the
 * ContextManager was initialized, providing proper thread safety without external
 * mutexes.
 *
 * Usage example:
 * ```cpp
 * QuoteBuffer buffer(ctx);
 * buffer.set("Hello, World!");
 * std::string content = buffer.get();
 * ```
 */
class QuoteBuffer final : public SyncBridge {

    std::string m_buffer{};  ///< The internal string buffer
    std::atomic<bool> busy_guard = false;

    /**
     * @struct BufferPayload
     * @brief Payload for buffer operations
     *
     * This structure carries the operation type and data for buffer modifications.
     * It is used with the SyncBridge pattern to ensure thread-safe access.
     */
    struct BufferPayload final : SyncPayload {
        enum Operation {
            SET,     ///< Set the buffer content
            GET,     ///< Get the buffer content
            APPEND,  ///< Append to the buffer content
        };

        Operation op;                ///< The operation to perform
        std::string data{};          ///< Data for SET operation
        std::string* result_ptr;     ///< Pointer to store the result for GET operation

        BufferPayload() : op(SET), result_ptr(nullptr) {}
    };

    /**
     * @brief Executes buffer operations in a thread-safe manner
     *
     * This method is called by the SyncBridge to perform modifications to the
     * buffer. It ensures that all modifications happen on the core where
     * the ContextManager was initialized, providing thread safety.
     *
     * @param payload Buffer operation instruction
     * @return PICO_OK on success, or error code on failure
     */
    uint32_t onExecute(SyncPayloadPtr payload) override;

public:
    /**
     * @brief Constructs a QuoteBuffer with the specified context manager
     *
     * @param ctx Shared context manager for synchronised execution
     */
    explicit QuoteBuffer(const ContextManagerPtr& ctx);

    /**
     * @brief Sets the buffer content from a std::string
     *
     * This method replaces the current buffer content with the provided string.
     * Thread-safe through SyncBridge integration, can be called from any core
     * or interrupt context.
     *
     * @param data String to set as the buffer content
     */
    void set(std::string data);

    /**
     * @brief Gets the buffer content
     *
     * This method returns a copy of the current buffer content.
     * Thread-safe through SyncBridge integration, can be called from any core
     * or interrupt context.
     *
     * @return Copy of the current buffer content
     */
    std::string get();

    /**
     * @brief Appends data to the buffer content
     *
     * This method adds the provided string to the end of the current buffer content.
     * Thread-safe through SyncBridge integration, can be called from any core
     * or interrupt context.
     *
     * @param data String to append to the buffer content
     */
    void append(std::string data);
};

} // namespace e5
