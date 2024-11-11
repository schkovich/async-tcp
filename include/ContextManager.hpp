// ContextManager.hpp

#pragma once

#include <pico/async_context_threadsafe_background.h>
#include "Worker.hpp"

namespace AsyncTcp {

    /**
     * @class ContextManager
     * @brief Manages the asynchronous context and worker scheduling within the AsyncTcp library.
     *
     * The `ContextManager` class is responsible for handling an asynchronous execution context,
     * including adding workers, managing locks, and setting work pending for specific workers.
     * This class enables thread-safe scheduling and context control, essential for asynchronous
     * TCP operations.
     */
    class ContextManager {
    public:
        /**
         * @brief Constructs a `ContextManager` instance.
         */
        ContextManager();

        /**
         * @brief Adds a worker to the default asynchronous context.
         *
         * @param worker Reference to the `Worker` instance to be added to the context.
         * @return `true` if the worker was successfully added, `false` otherwise.
         *
         * This function associates a `Worker` with the default context, enabling it to participate
         * in asynchronous operations managed by the `ContextManager`.
         */
        bool addWorker(Worker &worker) const;

        /**
         * @brief Retrieves a pointer to the default asynchronous context.
         *
         * @return Pointer to the `async_context_t` representing the default context.
         *
         * This function provides access to the internal context used for scheduling and
         * executing asynchronous operations.
         */
        [[nodiscard]] async_context_t *getDefaultContext() const;

        /**
         * @brief Acquires a lock on the context for thread-safe operations.
         *
         * This function locks the context, ensuring exclusive access for critical sections
         * or operations that should not be interrupted by other threads.
         */
        void acquireLock() const;

        /**
         * @brief Releases the lock on the context.
         *
         * This function unlocks the context, allowing other threads to access it.
         * It should be called after `acquireLock` to ensure proper access control.
         */
        void releaseLock() const;

        /**
         * @brief Marks a worker's work as pending within the context.
         *
         * @param worker Reference to the `Worker` instance for which work is set as pending.
         *
         * Signals that the specified worker has pending work to be processed within the context,
         * allowing the context manager to schedule it for execution.
         */
        void setWorkPending(Worker &worker) const;

        /**
         * @brief Initializes the default asynchronous context.
         *
         * @return `true` if the context was successfully initialized, `false` otherwise.
         *
         * This function sets up the default asynchronous context for the `ContextManager`,
         * ensuring it is ready to manage workers and handle asynchronous tasks.
         */
        bool initDefaultContext();

        /**
         * @brief Retrieves the core number associated with this `ContextManager`'s asynchronous context.
         *
         * @return The core number on which the context is running, or `-1` if the context is uninitialized.
         */
        uint getCore() const;

    private:
        async_context_threadsafe_background_t background_ctx = {}; /**< Thread-safe background context for asynchronous operations. */
        async_context_t *ctx = nullptr; /**< Pointer to the default asynchronous context. */
    };

} // namespace AsyncTcp
