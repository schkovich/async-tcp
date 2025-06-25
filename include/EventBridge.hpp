/**
 * @file EventBridge.hpp
 * @brief Defines the EventBridge class for bridging between C-style async context and C++ event handling.
 *
 * This file contains the EventBridge class which serves as a foundation for implementing
 * event handlers with proper core affinity in the AsyncTcpClient library. It provides
 * a clean interface between the C-style async_context API from the Pico SDK and
 * object-oriented C++ event handling.
 *
 * The EventBridge implements two types of worker patterns:
 *
 * 1. Persistent "when pending" workers - These remain registered with the context manager
 *    until explicitly removed. Their lifecycle is typically managed externally.
 *
 * 2. Ephemeral "at time" workers - These execute once at a specific time and are automatically
 *    removed from the context manager after execution. They can optionally manage their own
 *    lifecycle through self-ownership.
 *
 * The EventBridge follows the Template Method pattern, managing the registration and
 * lifecycle of workers with the async context while providing a virtual
 * method (onWork) for derived classes to implement specific event handling logic.
 *
 * Key features:
 * - Thread-safe execution with core affinity guarantees
 * - Automatic worker registration and cleanup
 * - Support for self-managed lifecycle for ephemeral workers
 * - Clean separation between async mechanism and business logic
 * 
 * @ingroup AsyncTCPClient
 */
#pragma once

#include <pico/async_context_threadsafe_background.h>

#include "ContextManager.hpp"
#include "EphemeralWorker.hpp"
#include "PerpetualWorker.hpp"

namespace AsyncTcp {

    // Forward declarations of the bridging function with C linkage
    extern "C" void perpetual_bridging_function(async_context_t* context, async_when_pending_worker_t* worker);
    extern "C" void ephemeral_bridging_function(async_context_t* context, async_work_on_timeout* worker);

    /**
     * @class EventBridge
     * @brief Bridges between the C-style async context API and C++ object-oriented event handling.
     *
     * The `EventBridge` class provides a foundation for implementing event handlers with proper
     * core affinity. It supports two types of workers:
     *
     * 1. Persistent "when pending" workers - These remain registered with the context manager
     *    until explicitly removed. Their lifecycle is typically managed externally.
     *
     * 2. Ephemeral "at time" workers - These execute once at a specific time and are automatically
     *    removed from the context manager after execution. They can optionally manage their own
     *    lifecycle through self-ownership.
     *
     * This class follows the Template Method pattern, where `doWork()` is the template method
     * that defines the algorithm structure, and `onWork()` is the hook method that derived
     * classes implement to provide specific behavior.
     *
     * Usage example for persistent worker:
     * ```cpp
     * class ConnectionHandler : public EventBridge {
     * protected:
     *     void onWork() override {
     *         // Handle connection event
     *     }
     * public:
     *     explicit ConnectionHandler(const ContextManagerPtr& ctx) : EventBridge(ctx) {}
     * };
     * ```
     *
     * Usage example for ephemeral worker:
     * ```cpp
     * class TimedTask : public EventBridge {
     * protected:
     *     void onWork() override {
     *         // Execute timed task
     *     }
     * public:
     *     explicit TimedTask(const ContextManagerPtr& ctx, EphemeralWorker worker)
     *         : EventBridge(ctx, worker) {}
     * };
     * ```
     */
    class EventBridge
    {
        // Friend function declaration with C linkage
        friend void perpetual_bridging_function(async_context_t* context, async_when_pending_worker_t* worker);
        friend void ephemeral_bridging_function(async_context_t* context, async_work_on_timeout* worker);
        PerpetualWorker m_perpetual_worker = {}; /**< Worker instance that interfaces with the async context */
        EphemeralWorker m_ephemeral_worker; /**< Ephemeral worker instance for timed execution */
        const ContextManagerPtr& m_ctx; /**< Reference to the context manager */
        std::unique_ptr<EventBridge> m_self = nullptr; /**< Self-reference for automatic cleanup */

        /**
         * @brief Executes the work by calling the virtual onWork method.
         *
         * This method is called by the bridging function when the worker is executed.
         * It cannot be overridden by derived classes, ensuring consistent execution flow.
         */
        void doWork();


        /**
         * @brief Releases ownership of self, transferring lifecycle management.
         *
         * This method is typically called in the bridging function to transfer ownership
         * to a local variable, which will destroy the EventBridge when it goes out of scope.
         *
         * @return A unique pointer to this EventBridge instance
         */
        std::unique_ptr<EventBridge> releaseOwnership();

    protected:
        /**
         * @brief Takes ownership of self, enabling self-managed lifecycle.
         *
         * This method is typically used with ephemeral workers to allow them to manage
         * their own lifecycle. When an ephemeral worker takes ownership of itself,
         * it will be automatically destroyed after its execution completes.
         *
         * @param self A unique pointer to this EventBridge instance
         */
        void takeOwnership(std::unique_ptr<EventBridge> self);

        /**
         * @brief Pure virtual method that derived classes must implement to define their event handling logic.
         *
         * This method is called when the worker is executed. Derived classes should implement
         * this method to perform their specific event handling task. It will be executed in the
         * context of the worker's core, ensuring proper core affinity.
         */
        virtual void onWork() = 0;

    public:
        /**
         * @brief Constructs an EventBridge instance for persistent "when pending" workers.
         *
         * Creates a Worker instance, sets up the bridging function, and registers
         * the worker with the context manager. The worker remains registered until
         * explicitly removed, typically by the destructor.
         *
         * @param ctx A reference to unique pointer to the context manager that will execute this worker
         */
        explicit EventBridge(const ContextManagerPtr& ctx);

        /**
         * @brief Constructs an EventBridge instance for ephemeral "at time" workers.
         *
         * Sets up the provided ephemeral worker with the bridging function. The worker
         * will be automatically removed from the context manager after execution.
         *
         * @param ctx A reference to unique pointer to the context manager that will execute this worker
         * @param worker The ephemeral worker to use
         */
        explicit EventBridge(const ContextManagerPtr& ctx, EphemeralWorker worker);

        /**
         * @brief Destructor that handles cleanup based on worker type.
         *
         * For persistent "when pending" workers, deregisters the worker from the context manager.
         * For ephemeral "at time" workers with self-ownership, no action is needed as they are
         * automatically removed by the async context after execution.
         */
        virtual ~EventBridge() = 0;

        /**
         * @brief Marks the persistent worker as having pending work to be executed.
         *
         * This method adds the worker to the async context's FIFO queue for execution.
         * The worker will be executed when the async context processes its queue,
         * maintaining proper core affinity.
         */
        void run();

        /**
         * @brief Schedules the ephemeral worker to run after the specified delay.
         *
         * This method schedules the worker to be placed in the async context's FIFO queue
         * after the specified delay in microseconds. Note that this does not guarantee
         * execution exactly at the specified time - the worker will be queued at that time
         * and executed when the async context processes its queue, maintaining proper core affinity.
         *
         * @param run_in The delay in microseconds after which to queue the worker for execution
         */
        void run(uint32_t run_in);
    };

} // namespace AsyncTcp
