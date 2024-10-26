// EventHandler.hpp
#pragma once

#include <memory>
#include "ContextManager.hpp"
#include "Worker.hpp"
#include "WorkerData.hpp"

namespace AsyncTcp {

    class AsyncTcpClient;        // Forward declaration

    /**
     * @class EventHandler
     * @brief Abstract base class for handling asynchronous TCP events.
     *
     * This class provides an interface for handling events within an asynchronous TCP client context.
     * Derived classes must implement the pure virtual `handleEvent()` function to specify the behavior
     * for event handling. The `EventHandler` instances are created using the static `create` factory method.
     */
    class EventHandler {

    public:
        /**
         * @brief Virtual destructor for safe cleanup in derived classes.
         */
        virtual ~EventHandler() = default;

        /**
         * @brief Pure virtual function to handle events.
         *
         * This function is meant to be overridden by derived classes to define custom event handling
         * logic within the asynchronous TCP client.
         */
        virtual void handleEvent() = 0;

        /**
         * @brief Factory method to create instances of `EventHandler` derived classes.
         *
         * @tparam T Derived class type of `EventHandler`.
         * @tparam Args Variadic template arguments for forwarding to the derived class constructor.
         * @param ctx Shared pointer to a `ContextManager` instance.
         * @param worker Shared pointer to a `Worker` instance.
         * @param args Additional arguments forwarded to the constructor of `T`.
         *
         * @return Shared pointer to an instance of the derived `EventHandler` class.
         *
         * This method creates and returns an instance of a concrete class derived from `EventHandler`.
         * The type `T` must inherit from `EventHandler`. The created instance is initialized with
         * the provided context and worker.
         */
        template <typename T, typename... Args>
        inline static std::shared_ptr<T> create(std::shared_ptr<ContextManager> ctx, std::shared_ptr<Worker> worker, Args&&... args) {
            static_assert(std::is_base_of<EventHandler, T>::value, "T must be derived from EventHandler");
            return std::shared_ptr<T>(new T(std::move(ctx), std::move(worker), std::forward<Args>(args)...));
        }

        /**
         * @brief Initialize the event handler with a specified client.
         *
         * @param client Reference to an `AsyncTcpClient` instance.
         *
         * This function sets the internal client pointer, allowing the event handler to
         * interact with the provided client during event handling.
         */
        inline void init(AsyncTcpClient &client) {
            _client = &client;  // Set client pointer
        }

    protected:
        /**
         * @brief Protected constructor to restrict instantiation to the factory method.
         *
         * @param ctx Shared pointer to a `ContextManager` instance.
         * @param worker Shared pointer to a `Worker` instance.
         *
         * The constructor initializes the context and worker members, setting up the `EventHandler`
         * for event handling. It is protected to enforce the use of the `create` method for
         * instantiation.
         */
        explicit EventHandler(std::shared_ptr<ContextManager> ctx, std::shared_ptr<Worker> worker)
                : _ctx(std::move(ctx)), _worker(std::move(worker)) {}

        std::shared_ptr<ContextManager> _ctx;    /**< Shared pointer to a `ContextManager` for managing the context. */
        std::shared_ptr<Worker> _worker;         /**< Shared pointer to a `Worker` for managing worker-specific tasks. */
        std::unique_ptr<WorkerData> _data;       /**< Unique pointer to `WorkerData`, containing core worker data. */
        AsyncTcpClient* _client = nullptr;       /**< Raw pointer to the `AsyncTcpClient` for client interactions. */
    };
}
