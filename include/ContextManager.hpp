/**
 * @file ContextManager.hpp
 * @brief Manages asynchronous context handling and worker scheduling in the AsyncTCP library.
 *
 * This file declares the ContextManager class, which encapsulates the setup and management of an
 * asynchronous execution context used throughout the library. Its responsibilities include:
 *   - Scheduling and executing work on the asynchronous context's core.
 *   - Adding and managing Workers for processing asynchronous tasks.
 *   - Coordinating thread-safe operations via lock acquisition and release.
 *   - Delegating synchronous work execution to the underlying async_context_execute_sync function.
 *
 * The public interface of ContextManager allows direct work execution via execWorkSynchronously,
 * and other essential context management operations.
 *
 * The HandlerFunction type, defined within this file, represents the callback signature for
 * synchronous work execution. It is a pointer to a function accepting a void* parameter and returning a uint32_t.
 *
 * @ingroup AsyncTCPClient
 */

#pragma once

#include <memory>
#include <pico/async_context_threadsafe_background.h>
#include "EphemeralWorker.hpp"
#include "Worker.hpp"

namespace AsyncTcp {

    /**
     * @brief Function pointer type for asynchronous work handlers.
     *
     * This type represents a pointer to a function that takes a void pointer as its argument
     * and returns an uint32_t. It is used as the callback signature for executing work synchronously.
     */
    typedef uint32_t (*HandlerFunction)(void* param);

    /**
     * @class ContextManager
     * @brief Manages the asynchronous context and worker scheduling within the AsyncTcp library.
     *
     * The ContextManager class is responsible for handling an asynchronous execution context,
     * including adding workers, managing locks, and setting work pending for specific workers.
     * It provides thread-safe scheduling and context control for asynchronous TCP operations.
     */
    class ContextManager {

        async_context_threadsafe_background_t background_ctx = {}; /**< Thread-safe background context for asynchronous operations. */
        async_context_t *ctx = nullptr;                           /**< Pointer to the default asynchronous context. */
        bool initiated = false;

    public:
        /**
         * @brief Constructs a ContextManager instance.
         */
        ContextManager();

        /**
         * @brief Destructor to deinitialize the background context.
         *
         * Cleans up any resources by calling `async_context_deinit`
         * if the context was successfully initialized. Ensures that no pending callbacks
         * remain by the time the destructor finishes.
         */
        ~ContextManager();

        /**
         * @brief Adds a worker to the default asynchronous context.
         *
         * @param worker Reference to the Worker instance to be added to the context.
         * @return true if the worker was successfully added, false otherwise.
         *
         * This function associates a Worker with the default context, enabling it to participate
         * in asynchronous operations managed by the ContextManager.
         */
        bool addWorker(Worker &worker) const;
        bool addWorker(async_when_pending_worker_t &worker) const;
        bool addEphemeralWorker(EphemeralWorker &worker, uint32_t delay = 0) const;

        bool removeWorker(Worker &worker) const;
        bool removeWorker(EphemeralWorker &worker) const;

        /**
         * @brief Marks a worker's work as pending within the context.
         *
         * @param worker Reference to the Worker instance for which work is set as pending.
         *
         * Signals that the specified Worker has pending work to be processed within the context,
         * allowing the ContextManager to schedule it for execution.
         */
        void setWorkPending(Worker &worker) const;

        void setWorkPending(async_when_pending_worker_t &worker) const;

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
         * It should be called after acquireLock to ensure proper access control.
         */
        void releaseLock() const;

        /**
         * @brief Initializes the default asynchronous context.
         *
         * @return true if the context was successfully initialized, false otherwise.
         *
         * This function sets up the default asynchronous context for the ContextManager,
         * ensuring it is ready to manage workers and handle asynchronous tasks.
         */
        bool initDefaultContext();

        /**
         * @brief Executes a function synchronously on the asynchronous context's core.
         *
         * This method is a thin wrapper around the async_context_execute_sync function,
         * and it directly forwards the provided callback and its parameter for synchronous execution.
         * The function is guaranteed to run on the same logical thread of execution as the asynchronous context.
         *
         * @note It is important that no locks are held on the asynchronous context when calling execWorkSynchronously,
         * as the underlying API requires a lock-free state for proper operation.
         *
         * @param handler Function pointer to the handler to execute. It must follow the signature defined by HandlerFunction.
         * @param param   Pointer to the parameters required by the handler.
         * @return The return value from the handler function.
         */
        [[nodiscard]] uint32_t execWorkSynchronously(const HandlerFunction& handler, void* param) const;

        /**
         * @brief Retrieves the core number associated with this ContextManager's asynchronous context.
         *
         * @return The core number on which the context is running, or -1 if the context is uninitialized.
         */
        [[nodiscard]] uint8_t getCore() const;

        void checkLock() const;

        void waitUntil(absolute_time_t until) const;

    };

    using ContextManagerPtr = std::unique_ptr<ContextManager>;

}