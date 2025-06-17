/**
 * @file SerialPrinter.hpp
 * @brief Asynchronous serial printing utility
 *
 * This file defines the SerialPrinter class which provides asynchronous
 * printing capabilities using the async context for proper core affinity.
 */

#pragma once
#include <string>
#include "ContextManager.hpp"

namespace e5 {

    using AsyncTcp::ContextManagerPtr;

    /**
     * @class SerialPrinter
     * @brief Provides asynchronous serial printing capabilities
     *
     * This class allows printing to the serial port from any core or interrupt context
     * by scheduling the actual printing operation on the appropriate core through
     * the async context.
     */
    class SerialPrinter
    {

        const ContextManagerPtr& m_ctx; ///< Context manager for scheduling print operations

    public:
        /**
         * @brief Constructs a SerialPrinter with the specified context manager
         *
         * @param ctx Shared context manager for synchronized execution
         */
        explicit SerialPrinter(const ContextManagerPtr& ctx);

        /**
         * @brief Prints a std::string to the serial port asynchronously
         *
         * This method schedules the printing operation to run on the core where
         * the context manager was initialized, ensuring thread safety.
         *
         * @param message std::string to print
         * @return PICO_OK on success, or error code on failure
         */
        uint32_t print(std::unique_ptr<std::string> message);
    };

} // namespace e5
