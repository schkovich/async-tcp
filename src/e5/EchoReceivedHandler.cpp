/**
 * @file EchoReceivedHandler.cpp
 * @brief Implementation of the handler for TCP client data received events.
 *
 * This file implements the EchoReceivedHandler class which handles data
 * received events from the echo server. Since Nagle's algorithm is disabled,
 * data naturally arrives in network-determined chunks.
 *
 * @author Goran
 * @date 2025-02-17
 * @ingroup AsyncTCPClient
 */

#include "e5/EchoReceivedHandler.hpp"
#include <string>

namespace e5 {

    /**
     * @brief Handles the data received event.
     *
     * This method is called when data is received on the TCP connection. It:
     * 1. Peeks at available data in the TCP buffer without consuming it
     * 2. Creates a string from the peeked data
     * 3. Outputs the data through the SerialPrinter with chunk information
     * 4. Consumes the data from the TCP buffer
     *
     * With Nagle's algorithm disabled, data arrives in multiple TCP
     * segments based on network conditions.
     */
    void EchoReceivedHandler::onWork() {
        const size_t available = m_io.peekAvailable();

        if (available == 0) return;  // No data to handle

        // Get direct access to data without consuming it
        const char* data = m_io.peekBuffer();

        // Create a unique_ptr to string directly
        auto content = std::make_unique<std::string>(data, available);

        // Look for and remove the End of Quote marker if present
        const std::string marker = "--- End of Quote ---";
        if (const size_t marker_pos = content->find(marker);
            marker_pos != std::string::npos) {
            // Remove the marker and create a clean output string
            *content = content->substr(0, marker_pos);
            DEBUGWIRE("Removed End of Quote marker from echo response\n");
        }

        // Pass the unique_ptr directly to the serial printer
        m_serial_printer.print(std::move(content));

        // Consume the data after printing to avoid duplicate processing
        m_io.peekConsume(available);
    }

} // namespace e5
