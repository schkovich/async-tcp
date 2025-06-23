/**
 * @file QotdReceivedHandler.hpp
 * @brief Defines a handler for Quote of the Day (QOTD) data received events.
 *
 * This file contains the QotdReceivedHandler class which implements the EventBridge
 * pattern to handle data received events for a QOTD client. When quote data is received,
 * this handler is triggered and processes the incoming data, storing it in a thread-safe
 * QuoteBuffer and simulating data processing.
 *
 * The handler demonstrates how to implement the EventBridge pattern for specific
 * event handling with proper core affinity.
 *
 * @author Goran
 * @date 2025-02-20
 * @ingroup AsyncTCPClient
 */

#pragma once
#include "QuoteBuffer.hpp"
#include "ContextManager.hpp"
#include "EventBridge.hpp"
#include "AsyncTcpClient.hpp"

namespace e5 {
    using namespace AsyncTcp;

/**
 * @class QotdReceivedHandler
 * @brief Handles the data received event for a Quote of the Day (QOTD) client.
 *
 * This handler is triggered when quote data is received on a TCP connection.
 * It implements the EventBridge pattern to ensure that the handling occurs
 * on the correct core with proper thread safety.
 *
 * The handler reads the received data, stores it in a thread-safe QuoteBuffer,
 * and simulates data processing through the simulateProcessData method.
 */
class QotdReceivedHandler final : public EventBridge
{
    QuoteBuffer& m_quote_buffer; /**< Reference to the thread-safe buffer where the quote will be stored. */
    AsyncTcpClient& m_io; /**< Reference to the TCP client handling the connection. */

protected:
    /**
     * @brief Handles the data received event.
     *
     * This method is called when data is received on the TCP connection. It reads
     * the available data, processes it using simulateProcessData, and stores it
     * in the provided thread-safe QuoteBuffer.
     *
     * This peek-based approach allows for partial data consumption, which is
     * important for protocols where message boundaries may not align with
     * TCP packet boundaries.
     *
     * The method is executed on the core where the ContextManager was initialized,
     * ensuring proper core affinity for non-thread-safe operations.
     */
    void onWork() override;

public:
    /**
     * @brief Constructs a QotdReceivedHandler.
     *
     * @param ctx Shared pointer to the context manager that will execute this handler
     * @param quote_buffer Reference to the thread-safe buffer where the quote will be stored
     * @param io Reference to the TCP client that received the data
     */
    explicit QotdReceivedHandler(const ContextManagerPtr& ctx, QuoteBuffer& quote_buffer, AsyncTcpClient& io) :
        EventBridge(ctx), m_quote_buffer(quote_buffer), m_io(io) {}
};

} // namespace e5