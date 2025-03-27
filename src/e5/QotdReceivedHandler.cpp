/**
 * @file QotdReceivedHandler.cpp
 * @brief Implementation of the handler for Quote of the Day (QOTD) data received events.
 *
 * This file implements the QotdReceivedHandler class which handles data
 * received events for a QOTD client. When quote data is received, this handler
 * processes the incoming data using a peek-based approach that demonstrates
 * how to handle partial data consumption.
 *
 * The implementation demonstrates how to use the EventBridge pattern to ensure
 * that data handling occurs on the correct core with proper thread safety.
 *
 * @author Goran
 * @date 2025-02-18
 * @ingroup AsyncTCPClient
 */

#include "e5/QotdReceivedHandler.hpp"

#include <atomic>

namespace e5 {

    /**
     * @brief Simulates processing of received data with different consumption patterns.
     *
     * This method demonstrates how to handle partial data consumption by simulating
     * different consumption patterns on successive calls:
     * - First call: consume half of the data
     * - Second call: consume all of the data
     * - Third call: consume none of the data
     * - Then repeat the pattern
     *
     * The static atomic counter ensures the pattern persists across multiple invocations,
     * simulating real-world scenarios where a protocol parser might not consume
     * all available data in a single call. The atomic type is used to ensure thread
     * safety in case this method is called from different contexts.
     *
     * @param data Pointer to the data to be processed
     * @param len Length of the data
     * @return The number of bytes consumed
     */
    size_t QotdReceivedHandler::simulateProcessData(const char* data, const size_t len) {
        (void)data; // Suppress unused parameter warning
        static std::atomic<int> call_count{0}; // Static to persist between calls, atomic for thread safety
        ++call_count;

        // Simulate different consumption patterns:
        // - First call: consume approximately half (using integer division which truncates)
        // - Second call: consume everything
        // - Third call: consume nothing
        // - Then repeat

        switch (call_count % 3) {
        case 0:
            return 0; // Consume nothing
        case 1:
            return len / 2; // Consume half (truncated for odd lengths)
        case 2:
            return len; // Consume all
        default:
            return 0;
        }
    }

    /**
     * @brief Handles the data received event.
     *
     * This method is called when data is received on the TCP connection. It:
     * 1. Peeks at the available data without consuming it
     * 2. Passes the data to simulateProcessData to determine how much to consume
     * 3. If data was consumed, marks it as consumed in the IO buffer
     * 4. Appends the consumed data to the thread-safe quote buffer
     *
     * This peek-based approach allows for partial data consumption, which is
     * important for protocols where message boundaries may not align with
     * TCP packet boundaries.
     *
     * The method is executed on the core where the ContextManager was initialized,
     * ensuring proper core affinity for non-thread-safe operations.
     */
    void QotdReceivedHandler::onWork() {
        const char* pb = m_io.peekBuffer();
        const size_t available = m_io.peekAvailable();

        if (const size_t consumed = simulateProcessData(pb, available); consumed > 0) {
            m_io.peekConsume(consumed);
            // m_quote_buffer.append(pb, consumed);
        }
    }

} // namespace e5