/**
 * @file PrintHandler.hpp
 * @brief Defines a handler for serial printing tasks.
 *
 * This file contains the PrintHandler class which implements the EventBridge
 * pattern to handle one-time printing operations. When triggered, this handler
 * prints a message to the serial output and then removes itself from the
 * SerialPrinter's task registry.
 *
 * The print_handler demonstrates how to implement the EventBridge pattern for
 * self-cleaning, one-shot operations with proper core affinity.
 *
 * @author Goran
 * @date 2025-02-20
 * @ingroup AsyncTCPClient
 */

#pragma once

#include <string>
#include "EventBridge.hpp"

namespace e5 {

    using namespace AsyncTcp;

    /**
     * @class PrintHandler
     * @brief Handles one-time serial printing operations.
     *
     * This handler is triggered to perform a single print operation to the
     * serial output. It implements the EventBridge pattern to ensure that
     * the printing occurs on the correct core with proper thread safety.
     *
     * After printing, the handler removes itself from the SerialPrinter's
     * task registry, demonstrating a self-cleaning pattern for one-shot operations.
     */
    class PrintHandler final : public EventBridge {

        std::unique_ptr<std::string> m_message = nullptr;            /**< Message buffer containing the text to print */
    protected:

        /**
         * @brief Handles the print operation.
         *
         * This method is called when the print_handler is executed. It prints the
         * stored message to the serial output and then removes itself from
         * the SerialPrinter's task registry.
         *
         * The method is executed on the core where the ContextManager was initialized,
         * ensuring proper core affinity for non-thread-safe operations like printing.
         */
        void onWork() override;

    public:

        /**
         * @brief Constructs a PrintHandler.
         *
         * @param ctx Shared pointer to the context manager that will execute this handler
         * @param message Message buffer containing the text to print
         */
        explicit PrintHandler(const ContextManagerPtr& ctx, std::unique_ptr<std::string> message);

        /**
         * @brief Static factory method that creates a PrintHandler with self-ownership
         *
         * Creates a PrintHandler instance, sets up self-ownership, and returns a pointer
         * to the instance. The instance will clean itself up after execution.
         *
         * @param ctx The context manager to use for scheduling
         * @param message The message buffer to print
         * @return A pointer to the created PrintHandler
         */
        static void create(const ContextManagerPtr& ctx, std::unique_ptr<std::string> message) {
            auto handler = std::make_unique<PrintHandler>(ctx, std::move(message));
            PrintHandler* raw_ptr = handler.get();
            raw_ptr->takeOwnership(std::move(handler));
            raw_ptr->run(0);
        }
    };

}