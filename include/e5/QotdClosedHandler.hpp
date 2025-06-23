/**
 * @file QotdClosedHandler.hpp
 * @brief Defines a handler for QOTD client connection closure events.
 *
 * This file contains the QotdClosedHandler class which implements the EventBridge
 * pattern to handle connection closure events for a Quote of the Day (QOTD) client.
 * When the server closes the connection (sends FIN), this handler is triggered and
 * processes the complete quote received, marking it as a complete transmission.
 *
 * The handler demonstrates how to implement the EventBridge pattern for specific
 * event handling with proper core affinity.
 *
 * @author Goran
 * @date 2025-06-22
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
 * @class QotdClosedHandler
 * @brief Handles the connection closure event for a QOTD client.
 *
 * Closing a TCP connection in the QOTD protocol indicates that the
 * server has transmitted the entire quote.
 * Clsoing the TCP connection triggers this handler,
 * It implements the EventBridge pattern ensuring that the handling occurs
 * on the correct core with proper thread safety.
 *
 * The handler finalises the quote buffer and notifies the user that a complete
 * quote has been received.
 */
class QotdClosedHandler final : public EventBridge {
    SerialPrinter& m_serial_printer; /**< Reference to the serial printer for output. */
    QuoteBuffer& m_quote_buffer; /**< Buffer storing the received quote data. */

protected:
    /**
     * @brief Handles the connection closure event.
     *
     * This method is called when the TCP connection is closed by the server.
     * It implements the specific logic for handling the connection closure event,
     * such as marking the quote as complete and notifying the user.
     *
     * In the QOTD protocol, the server sending FIN indicates that the complete
     * quote has been transmitted.
     *
     * The method is executed on the core where the ContextManager was initialized,
     * ensuring proper core affinity for non-thread-safe operations.
     */
    void onWork() override;

public:
    /**
     * @brief Constructs a QotdClosedHandler.
     *
     * @param ctx Shared pointer to the context manager that will execute this handler
     * @param quote_buffer Reference to the thread-safe buffer where the quote is stored
     * @param serial_printer Reference to the serial printer for output notifications
     */
    explicit QotdClosedHandler(const ContextManagerPtr& ctx,
                              QuoteBuffer& quote_buffer,
                              SerialPrinter& serial_printer) :
        EventBridge(ctx), m_serial_printer(serial_printer), m_quote_buffer(quote_buffer) {}
};

} // namespace e5
