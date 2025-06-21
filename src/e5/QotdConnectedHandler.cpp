/**
* @file QotdConnectedHandler.cpp
 * @brief Implementation of the handler for QOTD client connection events.
 *
 * This file implements the QotdConnectedHandler class which handles connection
 * established events for a Quote of the Day (QOTD) client. When a connection is established,
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

#include "e5/QotdConnectedHandler.hpp"
#include "Arduino.h"

namespace e5 {

    /**
     * @brief Handles the connection established event.
     *
     * This method is called when the TCP connection is established. It:
     * 1. Configures the connection to use keep-alive to maintain the connection
     * 2. Disables Nagle's algorithm by setting no delay to true for lower latency
     * 3. Retrieves the local IP address of the connection
     * 4. Formats and prints a message with the local IP address
     *
     * The method is executed on the core where the ContextManager was initialized,
     * ensuring proper core affinity for non-thread-safe operations like printing.
     */
    void QotdConnectedHandler::onWork() {
        // Configure connection parameters
        m_io.keepAlive();
        m_io.setNoDelay(true);  // Disable Nagle's algorithm for lower latency

        const std::string local_ip(m_io.localIP().toString().c_str());

        auto notify_connect = std::make_unique<std::string>("QOTD client connected. Local IP: " + local_ip + "\n");
        m_serial_printer.print(std::move(notify_connect));
    }

} // namespace e5