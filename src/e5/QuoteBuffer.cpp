/**
 * @file QuoteBuffer.cpp
 * @brief Implementation of the thread-safe buffer for storing and accessing the quote of the day
 *
 * This file implements the QuoteBuffer class which provides thread-safe access to
 * a string buffer using the SyncBridge pattern.
 *
 * @author Goran
 * @date 2025-02-20
 * @ingroup AsyncTCPClient
 */

#include <Arduino.h>
#include "e5/QuoteBuffer.hpp"

namespace e5 {

    /**
     * @brief Constructs a QuoteBuffer with the specified context manager
     *
     * @param ctx Shared context manager for synchronized execution
     */
    QuoteBuffer::QuoteBuffer(const ContextManagerPtr& ctx) : SyncBridge(ctx) {
        // Initialize with an empty string to ensure it's properly null-terminated
        m_buffer.clear();
    }

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
    uint32_t QuoteBuffer::onExecute(const SyncPayloadPtr payload) {

            switch (const auto* buffer_payload = static_cast<BufferPayload*>(payload.get()); buffer_payload->op) {
            case BufferPayload::SET:
                m_buffer = buffer_payload->data;
                return PICO_OK;

            case BufferPayload::APPEND:
                m_buffer += buffer_payload->data;
                return PICO_OK;

            default:
                if (buffer_payload->result_ptr) {
                    *buffer_payload->result_ptr = m_buffer;
                    return PICO_OK;
                }
                return PICO_ERROR_NO_DATA;
            }
    }

    /**
     * @brief Sets the buffer content
     *
     * This method replaces the current buffer content with the provided string.
     * Thread-safe through SyncBridge integration, can be called from any core
     * or interrupt context.
     *
     * @param data String to set as the buffer content
     */
    void QuoteBuffer::set(const std::string data) { // NOLINT
        if (bool expected = false; busy_guard.compare_exchange_strong(expected, true)) {
            auto payload = std::make_unique<BufferPayload>();
            payload->op = BufferPayload::SET;
            payload->data = data;
            if (const auto result = execute(std::move(payload)); result != PICO_OK) {
                DEBUGV("[c%d][%llu][ERROR] QuoteBuffer::set() returned error %d.\n",
                       rp2040.cpuid(), time_us_64(), result);
            }
            busy_guard = false;
        }
    }

    /**
     * @brief Gets the buffer content
     *
     * This method returns a copy of the current buffer content.
     * Thread-safe through SyncBridge integration, can be called from any core
     * or interrupt context.
     *
     * @return Copy of the current buffer content
     */
    std::string QuoteBuffer::get() { // NOLINT

        std::string result_string;
        if (bool expected = false; busy_guard.compare_exchange_strong(expected, true)) {
            auto payload = std::make_unique<BufferPayload>();
            payload->op = BufferPayload::GET;
            payload->result_ptr = &result_string;

            if (const auto result = execute(std::move(payload)); result != PICO_OK) {
                DEBUGV("[c%d][%llu][ERROR] QuoteBuffer::get() returned error %d.\n", rp2040.cpuid(), time_us_64(), result);
                busy_guard = false;
                return result_string;
            }

            busy_guard = false;
            return result_string;
        }
        return result_string;
    }

    /**
     * @brief Appends data to the buffer content
     *
     * This method adds the provided string to the end of the current buffer content.
     * Thread-safe through SyncBridge integration, can be called from any core
     * or interrupt context.
     *
     * @param data String to append to the buffer content
     */
    void QuoteBuffer::append(const std::string data) { // NOLINT
        if (bool expected = false; busy_guard.compare_exchange_strong(expected, true)) {
            auto payload = std::make_unique<BufferPayload>();
            payload->op = BufferPayload::APPEND;
            payload->data = data;
            if (const auto result = execute(std::move(payload)); result != PICO_OK) {
                DEBUGV("[c%d][%llu][ERROR] QuoteBuffer::append() returned error %d.\n",
                       rp2040.cpuid(), time_us_64(), result);
            }
            busy_guard = false;
        }
    }

} // namespace e5
