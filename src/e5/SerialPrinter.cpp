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

    // Print method implementation
    uint32_t SerialPrinter::print(const char* message) {
        MessageBuffer buf(message);
        DEBUGV("[c%d][%llu][INFO] SerialPrinter::print - message size: %d\n", rp2040.cpuid(), time_us_64(), buf.size());

        // Create handler using factory method
        const auto handler = PrintHandler::create(m_ctx, std::move(buf));

        // Take ownership and schedule for immediate execution
        DEBUGV("[c%d][%llu][INFO] SerialPrinter::print - handler %p created successfully\n",
            rp2040.cpuid(), time_us_64(), handler);

        // Schedule the print handler to run immediately
            handler->run(7);
            return PICO_OK; // Return success code
    }

} // namespace e5
