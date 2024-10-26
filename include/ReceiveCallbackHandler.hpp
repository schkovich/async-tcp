// ReceiveCallbackHandler.hpp

#pragma once
#include "EventHandler.hpp"

namespace AsyncTcp {

    /**
     * @class ReceiveCallbackHandler
     * @brief Concrete implementation of `EventHandler` for handling receive callback events.
     *
     * This class provides an implementation of the `handleEvent` method to manage events
     * related to receiving data in an asynchronous TCP context.
     */
    class ReceiveCallbackHandler : public EventHandler {

    public:
        /**
         * @brief Constructs a `ReceiveCallbackHandler` with a specified context and worker.
         *
         * @param ctx Shared pointer to a `ContextManager` instance.
         * @param worker Shared pointer to a `Worker` instance.
         *
         * Initializes the `ReceiveCallbackHandler` with the provided context and worker.
         * This setup allows the handler to manage and process data reception events.
         */
        explicit ReceiveCallbackHandler(std::shared_ptr<ContextManager> ctx, std::shared_ptr<Worker> worker)
                : EventHandler(std::move(ctx), std::move(worker)) {}

        /**
         * @brief Overrides `EventHandler::handleEvent` to define event handling for received data.
         *
         * This function is called when data is received, and it contains logic to process
         * the incoming data within the asynchronous TCP client context.
         */
        void handleEvent() override;
    };

} // namespace AsyncTcp
