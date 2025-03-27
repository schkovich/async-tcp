/**
* @file SerialPrinter.hpp
 * @brief Asynchronous serial printing utility
 *
 * This class provides asynchronous printing capabilities using the async context
 * to ensure proper core affinity for serial operations.
 */

#pragma once

#include <atomic>
#include <memory>
#include "ContextManager.hpp"

namespace e5 {

    class SerialPrinter {
    public:
        /**
         * @brief Constructs a SerialPrinter instance
         * @param ctx The context manager to use for scheduling print operations
         */
        explicit SerialPrinter(const AsyncTcp::ContextManagerPtr& ctx);

        /**
         * @brief Prints a message asynchronously
         *
         * This method creates a PrintHandler to process the message and schedules it
         * for immediate execution on the appropriate core.
         *
         * @param message The message to print
         * @return PICO_OK on success, or an error code on failure
         */
        uint32_t print(const char* message);

    private:
        const AsyncTcp::ContextManagerPtr& m_ctx; // Context manager for scheduling

        std::atomic<bool> busy = false;

    };

} // namespace e5