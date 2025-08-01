/**
 * @file EventBridge.cpp
 * @brief Implementation of the EventBridge class for bridging between C-style
 * async context and C++ event handling.
 *
 * This file implements the EventBridge class, which provides a foundation for event handlers with proper core affinity in the AsyncTcpClient library.
 * It supports explicit initialization and registration of worker types:
 *
 * 1. Persistent "when pending" workers - Registered via initialisePerpetualBridge().
 *    These remain registered with the context manager until explicitly removed. Registration is no longer automatic in the constructor.
 *
 * 2. Ephemeral "at time" workers - Registered via initialiseEphemeralBridge().
 *    These execute once at a specific time and are automatically removed from the context manager after execution. They can optionally manage their own lifecycle through self-ownership.
 *
 * The implementation manages worker registration, lifecycle, and event execution, following the Template Method pattern. It provides thread-safe execution with core affinity guarantees and a clean separation between async mechanisms and business logic.
 *
 * @ingroup AsyncTCPClient
 */

#include <Arduino.h>

#include "ContextManager.hpp"
#include "EventBridge.hpp"

namespace async_tcp {

    /**
     * @brief Constructs an EventBridge instance for persistent "when pending"
     * workers.
     *
     * Creates a Worker instance, sets up the bridging function, and registers
     * the worker with the context manager. The worker remains registered until
     * explicitly removed, typically by the destructor.
     *
     * @param ctx Shared pointer to the context manager that will execute this
     * worker
     */
    EventBridge::EventBridge(const AsyncCtx &ctx) : m_ctx(ctx) {}

    /**
     * @brief Destructor that handles cleanup based on worker type.
     *
     * For persistent "when pending" workers, deregisters the worker from the
     * context manager. For ephemeral "at time" workers with self-ownership, no
     * action is needed as they are automatically removed by the async context
     * after execution.
     */
    EventBridge::~EventBridge() {

        if (m_self == nullptr) {
            // Check if m_worker was explicitly initialized (indicating a
            // persistent worker)
            if (m_perpetual_worker.getWorker()->do_work != nullptr) {
                m_ctx.removeWorker(m_perpetual_worker);
            }
        }
        // Otherwise, m_worker is in its default state, so this was an ephemeral
        // worker
    }

    /**
     * @brief Marks the worker as having pending work to be executed.
     *
     * This method adds the worker to the async context's FIFO queue for
     * execution. The worker will be executed when the async context processes
     * its queue, maintaining proper core affinity.
     */
    void EventBridge::run() { m_ctx.setWorkPending(m_perpetual_worker); }

    /**
     * @brief Schedules the ephemeral worker to run after the specified delay.
     *
     * This method schedules the worker to be placed in the async context's FIFO
     * queue after the specified delay in microseconds. Note that this does not
     * guarantee execution exactly at the specified time - the worker will be
     * queued at that time and executed when the async context processes its
     * queue, maintaining proper core affinity.
     *
     * @param run_in The delay in microseconds after which to queue the worker
     * for execution
     */
    void EventBridge::run(const uint32_t run_in) { // NOLINT
        if (const auto result = m_ctx.addWorker(m_ephemeral_worker, run_in);
            !result) {
            DEBUGV("[c%d][%llu][ERROR] EventBridge::run - Failed to add "
                   "ephemeral worker: %p, error: %lu\n",
                   rp2040.cpuid(), time_us_64(), this, result);
        }
    }

    /**
     * @brief Executes the work by calling the virtual onWork method.
     *
     * This method is called by the bridging function when the worker is
     * executed. It cannot be overridden by derived classes, ensuring consistent
     * execution flow. This is the template method in the Template Method
     * pattern.
     */
    void EventBridge::doWork() { onWork(); }

    /**
     * @brief Takes ownership of self, enabling self-managed lifecycle.
     *
     * This method is typically used with ephemeral workers to allow them to
     * manage their own lifecycle. When an ephemeral worker takes ownership of
     * itself, it will be automatically destroyed after its execution completes.
     *
     * @param self A unique pointer to this EventBridge instance
     */
    void EventBridge::takeOwnership(std::unique_ptr<EventBridge> self) {
        m_self = std::move(self);
    }

    /**
     * @brief Releases ownership of self, transferring lifecycle management.
     *
     * This method is typically called in the bridging function to transfer
     * ownership to a local variable, which will destroy the EventBridge when it
     * goes out of scope. This is crucial for self-cleaning ephemeral workers.
     *
     * @return A unique pointer to this EventBridge instance
     */
    std::unique_ptr<EventBridge> EventBridge::releaseOwnership() {
        return std::move(m_self);
    }

    /**
     * @brief Bridging function that connects the C-style callback to the C++
     * object.
     *
     * This function is called by the async context when a persistent worker is
     * pending execution. It retrieves the EventBridge instance from the
     * worker's user_data and calls its doWork method. The extern "C" linkage
     * ensures correct calling convention for the C-based SDK.
     *
     * @param context The async context that is executing the worker
     * @param worker The async_when_pending_worker_t that is being executed
     */
    void
    perpetual_bridging_function(async_context_t *context,
                                async_when_pending_worker_t *worker) { // NOLINT
        (void)context; // Unused parameter

        if (worker && worker->user_data) {
            // Direct cast to EventBridge* since we're storing the EventBridge
            // instance directly
            static_cast<EventBridge *>(worker->user_data)->doWork();
        } else {
            DEBUGV(
                "[AC-%d][%llu][ERROR] EventBridge::perpetual_bridging_function "
                "- invalid worker or user data\n",
                rp2040.cpuid(), time_us_64());
        }
    }

    /**
     * @brief Bridging function that connects the C-style callback to the C++
     * object for ephemeral workers.
     *
     * This function is called by the async context when an ephemeral worker's
     * time arrives. It retrieves the EventBridge instance from the worker's
     * user_data, calls its doWork method, and then releases ownership to
     * trigger cleanup when the function returns.
     *
     * This function also handles queue monitoring for performance tracking, and
     * toggles the LED_BUILTIN pin to provide visual feedback during worker
     * execution.
     *
     * Note: By the time this function is called, the async context has already
     * removed the worker from its internal list, so no explicit removal is
     * needed.
     *
     * @param context The async context that is executing the worker
     * @param worker The async_work_on_timeout that is being executed
     */
    void ephemeral_bridging_function(async_context_t *context,
                                     async_work_on_timeout *worker) { // NOLINT
        (void)context;
        if (worker && worker->user_data) {
            const auto pBridge = static_cast<EventBridge *>(worker->user_data);
            pBridge->doWork();
            pBridge->releaseOwnership();
        } else {
            DEBUGV("\033[1;31m[AC-%d][%llu][ERROR] "
                   "EventBridge::ephemeral_bridging_function - invalid worker "
                   "or user data\033[1;37m\n",
                   rp2040.cpuid(), time_us_64());
        }
    }

} // namespace AsyncTcp
