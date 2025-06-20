/**
 * @file EchoConnectedHandler.cpp
 * @brief Implementation of the handler for TCP client connection events.
 *
 * This file implements the EchoConnectedHandler class which handles connection
 * established events for an echo client. When a connection is established,
 * this handler configures the connection parameters and notifies the user
 * through the serial printer.
 *
 * The implementation demonstrates how to use the EventBridge pattern to ensure
 * that connection handling occurs on the correct core with proper thread safety.
 *
 * @author Goran
 * @date 2025-02-19
 * @ingroup AsyncTCPClient
 */

#include "e5/EchoConnectedHandler.hpp"
#include "Arduino.h"

namespace e5 {

/**
 * @brief Handles the connection established event.
 *
 * This method is called when the TCP connection is established. It:
 * 1. Configures the connection to use keep-alive to maintain the connection
 * 2. Enables Nagle's algorithm
 * 3. Retrieves the local IP address of the connection
 * 4. Formats and prints a message with the local IP address
 *
 * The method is executed on the core where the ContextManager was initialized,
 * ensuring proper core affinity for non-thread-safe operations like printing.
 */
void EchoConnectedHandler::onWork() {
    // Configure connection parameters
    m_io.keepAlive();
    m_io.setNoDelay(false);  // Enable Nagle's algorithm for better bandwidth efficiency

    // Get the local IP address
    const auto ip = m_io.localIP().toString().c_str();
    auto local_ip = std::make_unique<std::string>("Echo client connected. Local IP: " + std::string(ip));

    // Print the message using the thread-safe SerialPrinter
    m_serial_printer.print(std::move(local_ip));
}

}