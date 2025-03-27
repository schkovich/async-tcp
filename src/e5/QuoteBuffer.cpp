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

#include "e5/QuoteBuffer.hpp"

#include "e5/LedDebugger.hpp"

namespace e5 {

    /**
     * @brief Constructs a QuoteBuffer with the specified context manager
     *
     * @param ctx Shared context manager for synchronized execution
     */
    QuoteBuffer::QuoteBuffer(const ContextManagerPtr& ctx) : SyncBridge(ctx) {}

    /**
     * @brief Executes buffer operations in a thread-safe manner
     *
     * This method is called by the SyncBridge to perform modifications to the
     * buffer. It ensures that all modifications happen on the core where
     * the ContextManager was initialized, providing thread safety.
     *
     * @param payload Buffer operation instruction
     * @return PICO_OK on success, or 1 if buffer is empty, 0 if not empty (for EMPTY operation)
     */
    uint32_t QuoteBuffer::onExecute(const SyncPayloadPtr payload) {

            switch (auto* buffer_payload = static_cast<BufferPayload*>(payload.get()); buffer_payload->op) {
            case BufferPayload::SET:
                DEBUGV("[c%d][%llu][INFO] QuoteBuffer::onExecute - SET\n", rp2040.cpuid(), time_us_64());
                if (m_buffer.empty()) {
                    m_buffer = std::move(buffer_payload->data);
                    return PICO_OK;
                }
                return -1; // Return -1 if buffer is not empty

            case BufferPayload::GET:
                DEBUGV("[c%d][%llu][INFO] QuoteBuffer::onExecute - GET\n", rp2040.cpuid(), time_us_64());
                if (!m_buffer.empty()) {
                    if (buffer_payload->result_ptr) {
                        *buffer_payload->result_ptr = m_buffer;
                        return PICO_OK;
                    }
                    return -2; // Return -2 if there was no result pointer
                }
                return PICO_OK; // Return PICO_OK if buffer is empty
            }

            return PICO_OK;
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
    void QuoteBuffer::set(const std::string& data) {
        if (bool expected = false; busy_guard.compare_exchange_strong(expected, true)) {
            auto payload = std::make_unique<BufferPayload>();
            payload->op = BufferPayload::SET;
            payload->data = data;
            execute(std::move(payload));
            busy_guard = false;
        } else {
            DEBUGV("[c%d][%llu][INFO] QuoteBuffer::set() - LOCKED\n",
                rp2040.cpuid(), time_us_64());
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
    std::string QuoteBuffer::get() {

        if (bool expected = false; busy_guard.compare_exchange_strong(expected, true)) {
            // lockBridge();
            std::string result;
            auto payload = std::make_unique<BufferPayload>();
            payload->op = BufferPayload::GET;
            payload->result_ptr = &result;
            execute(std::move(payload));
            // unlockBridge();
            busy_guard = false;
            return result;
        }

        return std::string{}; // Return empty string if busy
    }

} // namespace e5
