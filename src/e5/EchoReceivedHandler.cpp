/**
 * @file EchoReceivedHandler.cpp
 * @brief Implementation of the handler for TCP client data received events.
 *
 * This file implements the EchoReceivedHandler class which handles data
 * received events for an echo client. When data is received, this handler
 * reads the data, reverses it, and outputs it through the serial printer.
 *
 * The implementation demonstrates how to use the EventBridge pattern to ensure
 * that data handling occurs on the correct core with proper thread safety.
 *
 * @author Goran
 * @date 2025-02-17
 * @ingroup AsyncTCPClient
 */

#include "e5/EchoReceivedHandler.hpp"
#include <algorithm>
#include <string>

namespace e5 {

    /**
     * @brief Handles the data received event.
     *
     * This method is called when data is received on the TCP connection. It:
     * 1. Determines a safe buffer size based on available data and maximum size
     * 2. Reads the available data into a buffer
     * 3. Creates a string from the buffer
     * 4. Reverses the string content
     * 5. Outputs the reversed string through the SerialPrinter
     *
     * The method is executed on the core where the ContextManager was initialized,
     * ensuring proper core affinity for non-thread-safe operations like printing.
     */
    void EchoReceivedHandler::onWork() {
        // Use the minimum of available data and the maximum size constant
        const size_t safe_size = std::min(m_io.available(), MAX_QOTD_SIZE);

        // Create a buffer of the safe size and read data into it
        char buffer[safe_size];
        const size_t count = m_io.read(buffer, safe_size);
        (void)count; // Suppress unused variable warning

        // Create a string from the buffer, reverse it, and print it
        std::string reversed_quote = buffer;
        std::reverse(reversed_quote.begin(), reversed_quote.end());
        // m_serial_printer.print(reversed_quote.c_str());
    }

} // namespace e5
