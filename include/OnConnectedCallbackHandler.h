// OnConnectedCallbackHandler.hpp
#pragma once
#include "AsyncTcpClient.hpp"
#include "EventHandler.hpp"

namespace AsyncTcp {

/**
 * @class OnConnectedCallbackHandler
 * @brief Concrete implementation of `EventHandler` for handling connection
 * established events.
 *
 * This class provides an implementation of the `handleEvent` method to manage
 * events related to successful TCP connections in an asynchronous context.
 */
class OnConnectedCallbackHandler final : public EventHandler {
public:
    /**
     * @brief Constructs an `OnConnectedCallbackHandler` with a specified context,
     * worker, and client.
     *
     * @param ctx Shared pointer to a `ContextManager` instance.
     * @param worker Shared pointer to a `Worker` instance.
     * @param client Reference to the AsyncTcpClient instance that handles TCP
     * operations.
     */
    explicit OnConnectedCallbackHandler(
        std::shared_ptr<ContextManager> ctx,
        std::shared_ptr<Worker> worker,
        AsyncTcpClient& client)
        : EventHandler(std::move(ctx), std::move(worker))
        , _client(client) {}

    /**
     * @brief Handles connection established events.
     *
     * This implementation creates a simple notification message and schedules
     * it for asynchronous processing.
     */
    void handleEvent() override;

private:
    AsyncTcpClient& _client; /**< Reference to the TCP client handling the connection. */
};

} // namespace AsyncTcp