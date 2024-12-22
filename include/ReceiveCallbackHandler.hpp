// ReceiveCallbackHandler.hpp

#pragma once
#include "AsyncTcpClient.hpp"
#include "EventHandler.hpp"

namespace AsyncTcp {

/**
 * @class ReceiveCallbackHandler
 * @brief Concrete implementation of `EventHandler` for handling receive
 * callback events.
 *
 * This class provides an implementation of the `handleEvent` method to manage
 * events related to receiving data in an asynchronous TCP context.
 */
class ReceiveCallbackHandler final : public EventHandler {

  public:
    /**
     * @brief Constructs a `ReceiveCallbackHandler` with a specified context,
     * worker, and client.
     *
     * @param ctx Shared pointer to a `ContextManager` instance.
     * @param worker Shared pointer to a `Worker` instance.
     * @param client Reference to the AsyncTcpClient instance that handles TCP
     * operations.
     */
    explicit ReceiveCallbackHandler(std::shared_ptr<ContextManager> ctx,
                                    std::shared_ptr<Worker> worker,
                                    AsyncTcpClient &client)
        : EventHandler(std::move(ctx), std::move(worker)), client(client) {}

    /**
     * @brief Handles receive events for incoming TCP data.
     *
     * This implementation processes incoming data by creating appropriate
     * worker data and scheduling it for asynchronous processing.
     */
    void handleEvent() override;

  private:
    AsyncTcpClient
        &client; /**< Reference to the TCP client handling the connection. */
};

} // namespace AsyncTcp