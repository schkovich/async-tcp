/**
 * @file SerialPrinter.cpp
 * @brief Implementation of asynchronous serial printing utility
 *
 * This file implements the SerialPrinter class which provides asynchronous
 * printing capabilities using the async context for proper core affinity.
 */

#include "e5/SerialPrinter.hpp"
#include "Arduino.h"
#include "ContextManager.hpp"
#include "e5/MessageBuffer.hpp"
#include "e5/PrintHandler.hpp"

namespace e5 {

    // Constructor implementation
    SerialPrinter::SerialPrinter(const ContextManagerPtr& ctx) : m_ctx(ctx) {}

    // Print method implementation for std::string
    uint32_t SerialPrinter::print(std::unique_ptr<std::string> message) {
        // if (message.empty()) {
        //     DEBUGV("[c%d][%llu][ERROR] SerialPrinter::print - null message pointer\n", rp2040.cpuid(), elapsed());
        //     return PICO_ERROR_INVALID_DATA;
        // }
        // Create and run handler using factory method
        PrintHandler::create(m_ctx, std::move(message));
        return PICO_OK; // Return success code

    }

} // namespace e5
