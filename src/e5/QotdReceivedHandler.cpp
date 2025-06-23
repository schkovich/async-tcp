/**
 * @file QotdReceivedHandler.cpp
 * @brief Implementation of the handler for Quote of the Day (QOTD) data received events.
 *
 * This file implements the QotdReceivedHandler class which handles data
 * received events for a QOTD client. When quote data is received, this handler
 * processes the incoming data and stores it in a thread-safe buffer.
 *
 * The implementation demonstrates how to use the EventBridge pattern to ensure
 * that data handling occurs on the correct core with proper thread safety.
 *
 * @author Goran
 * @date 2025-02-18
 * @ingroup AsyncTCPClient
 */

#include "e5/QotdReceivedHandler.hpp"

namespace e5 {

    /**
     * @brief Handles the data received event.
     *
     * This method is called when data is received on the TCP connection. It:
     * 1. Peeks at the available data without consuming it
     * 2. Appends the data to the thread-safe quote buffer
     * 3. Marks the data as consumed in the IO buffer
     *
     * Since the QOTD server likely has Nagle's algorithm enabled, we expect
     * to receive the complete quote in a single chunk.
     *
     * The method executes on the core where the ContextManager initiated the
     * async_context_t, ensuring proper core affinity for non-thread-safe operations.
     */
    void QotdReceivedHandler::onWork() {
        // Check for available data using peekAvailable

        if (const size_t available = m_io.peekAvailable(); available > 0) {

            if (const char *peek_buffer = m_io.peekBuffer();
                peek_buffer != nullptr) {
                // Create a string from the peeked data
                // This makes a copy of the data, so the original is safe to consume.
                const std::string data(peek_buffer, available);

                m_quote_buffer.append(data);

                // Mark the data as consumed in the TCP buffer
                m_io.peekConsume(available);

                DEBUGWIRE("[QOTD] Received %zu bytes: %.*s\n",
                       available,
                       std::min(static_cast<size_t>(32), available),  // Show at most 32 chars
                       peek_buffer);
            }
        }
    }

} // namespace e5