/**
 * @file EchoConnectedHandler.hpp
 * @brief Defines a handler for TCP client connection events.
 *
 * This file contains the EchoConnectedHandler class which implements the EventBridge
 * pattern to handle connection events for an echo client. When a connection is established,
 * this handler is triggered and can perform actions such as sending data or notifying
 * the user through the serial printer.
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
#include "SerialPrinter.hpp"

namespace e5 {
    using namespace AsyncTcp;

/**
 * @class EchoConnectedHandler
 * @brief Handles the connection established event for an echo client.
 *
 * This handler is triggered when a TCP connection is successfully established
 * for an echo client. It implements the EventBridge pattern to ensure that
 * the handling occurs on the correct core with proper thread safety.
 *
 * The handler can access the TCP client to send data or perform other operations,
 * and can use the SerialPrinter to output status messages.
 */
class EchoConnectedHandler final : public EventBridge {
    AsyncTcpClient& m_io; /**< Reference to the TCP client handling the connection. */
    SerialPrinter& m_serial_printer; /**< Reference to the serial printer for output. */

protected:
    /**
     * @brief Handles the connection established event.
     *
     * This method is called when the TCP connection is established. It implements
     * the specific logic for handling the connection event, such as sending initial
     * data or notifying the user through the serial printer.
     *
     * The method is executed on the core where the ContextManager was initialized,
     * ensuring proper core affinity for non-thread-safe operations.
     */
    void onWork() override;

public:
    /**
     * @brief Constructs an EchoConnectedHandler.
     *
     * @param ctx Shared pointer to the context manager that will execute this handler
     * @param io Reference to the TCP client that established the connection
     * @param serial_printer Reference to the serial printer for output messages
     */
    explicit EchoConnectedHandler(const ContextManagerPtr& ctx, AsyncTcpClient& io, SerialPrinter& serial_printer) :
        EventBridge(ctx), m_io(io), m_serial_printer(serial_printer) {}
};

}