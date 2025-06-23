/**
 * @file PrintHandler.cpp
 * @brief Implementation of the handler for serial printing tasks.
 *
 * This file implements the PrintHandler class which handles one-time
 * printing operations to the serial output. When triggered, this handler
 * prints a message and then removes itself from the SerialPrinter's task registry.
 *
 * The implementation demonstrates how to use the EventBridge pattern for
 * self-cleaning, one-shot operations with proper core affinity.
 *
 * @author Goran
 * @date 2025-02-20
 * @ingroup AsyncTCPClient
 */

#include "e5/PrintHandler.hpp"
#include <iostream>

#include "Arduino.h"

namespace e5 {

    /**
     * @brief Handles the print operation.
     *
     * This method is called when the handler is executed. It:
     * 1. Retrieves the message from the message buffer
     * 2. Prints the message to the serial output if it's not empty
     * 3. Removes itself from the SerialPrinter's task registry using its own address as the key
     *
     * The self-removal pattern demonstrates how to implement one-shot operations
     * that clean up after themselves. When the task is removed from the SerialPrinter,
     * the unique_ptr owning this handler is destroyed, which triggers the handler's
     * destructor and deregisters it from the async context.
     *
     * The method is executed on the core where the ContextManager was initialized,
     * ensuring proper core affinity for non-thread-safe operations like printing.
     */
    void PrintHandler::onWork() {
        if (!m_message->empty()) {
            Serial1.println(m_message->c_str());
            m_message.reset();
        }
    }

    /**
     * @brief Constructs a PrintHandler with the specified context, message, and printer.
     *
     * @param ctx Context manager for execution
     * @param worker
     * @param message Message to print
     */
    PrintHandler::PrintHandler(const ContextManagerPtr& ctx, EphemeralWorker worker, std::unique_ptr<std::string> message) :
        EventBridge(ctx, std::move(worker)), m_message(std::move(message)) {}
} // namespace e5