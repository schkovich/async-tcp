/**
 * @file QotdConnectedHandler.hpp
 * @brief Defines a handler for QOTD client connection events.
 *
 * This file contains the QotdConnectedHandler class which implements the EventBridge
 * pattern to handle connection events for a Quote of the Day (QOTD) client. When a
 * connection is established, this handler is triggered and can configure the connection
 * parameters and notify the user through the serial printer.
 *
 * The handler demonstrates how to implement the EventBridge pattern for specific
 * event handling with proper core affinity.
 *
 * @author Goran
 * @date 2025-02-19
 * @ingroup AsyncTCPClient
 */

#pragma once
#include "AsyncTcpClient.hpp"
#include "ContextManager.hpp"
#include "EventBridge.hpp"
#include "QuoteBuffer.hpp"
#include "SerialPrinter.hpp"

namespace e5 {
    using namespace AsyncTcp;

/**
 * @class QotdConnectedHandler
 * @brief Handles the connection established event for a QOTD client.
 *
 * This handler is triggered when a TCP connection is successfully established
 * for a Quote of the Day (QOTD) client. It implements the EventBridge pattern
 * to ensure that the handling occurs on the correct core with proper thread safety.
 *
 * The handler configures the connection for optimal QOTD protocol performance by
 * enabling keep-alive and disabling Nagle's algorithm for lower latency.
 */
class QotdConnectedHandler final : public EventBridge {
    AsyncTcpClient& m_io; /**< Reference to the TCP client handling the connection. */
    SerialPrinter& m_serial_printer; /**< Reference to the serial printer for output. */
    QuoteBuffer& m_quote_buffer; /**< Buffer for storing the quote data. */

protected:
    /**
     * @brief Handles the connection established event.
     *
     * This method is called when the TCP connection is established. It implements
     * the specific logic for handling the connection event, such as configuring
     * connection parameters and notifying the user through the serial printer.
     *
     * The method is executed on the core where the ContextManager was initialized,
     * ensuring proper core affinity for non-thread-safe operations.
     */
    void onWork() override;

public:
    /**
     * @brief Constructs a QotdConnectedHandler.
     *
     * @param ctx Shared pointer to the context manager that will execute this handler
     * @param io Reference to the TCP client that established the connection
     * @param serial_printer Reference to the serial printer for output messages
     * @param quote_buffer
     */
    explicit QotdConnectedHandler(const ContextManagerPtr& ctx, AsyncTcpClient& io, SerialPrinter& serial_printer, QuoteBuffer& quote_buffer) :
        EventBridge(ctx), m_io(io), m_serial_printer(serial_printer), m_quote_buffer(quote_buffer) {}
};

} // namespace e5