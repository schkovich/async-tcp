/**
 * @file EventBridge.cpp
 * @brief Implementation of the EventBridge class for bridging between C-style async context and C++ event handling.
 *
 * This file implements the EventBridge class which provides a foundation for event handlers
 * with proper core affinity. It supports two types of workers:
 *
 * 1. Persistent "when pending" workers - These remain registered with the context manager
 *    until explicitly removed. Their lifecycle is typically managed externally.
 *
 * 2. Ephemeral "at time" workers - These execute once at a specific time and are automatically
 *    removed from the context manager after execution. They can optionally manage their own
 *    lifecycle through self-ownership.
 *
 * The implementation handles:
 * - Worker creation and registration with the context manager
 * - Setting up the bridging functions for C to C++ callbacks
 * - Proper cleanup when the EventBridge is destroyed
 * - Triggering work execution with core affinity guarantees
 */

#include "EventBridge.hpp"
#include "ContextManager.hpp"
#include "e5/QueueMonitor.hpp"
#include "e5/WorkerCounter.hpp"

namespace e5 {
    QueueMonitor g_queueMonitor;
}

namespace AsyncTcp {

    /**
     * @brief Constructs an EventBridge instance for persistent "when pending" workers.
     *
     * Creates a Worker instance, sets up the bridging function, and registers
     * the worker with the context manager. The worker remains registered until
     * explicitly removed, typically by the destructor.
     *
     * @param ctx Shared pointer to the context manager that will execute this worker
     */
    EventBridge::EventBridge(const ContextManagerPtr& ctx) : m_ctx(ctx) {
        m_worker = Worker();
        m_worker.setWorkFunction(&worker_bridging_function);
        // Store a direct pointer to this EventBridge instance
        m_worker.setWorkerData(this);
        m_ctx->addWorker(m_worker);
    }

    /**
     * @brief Constructs an EventBridge instance for ephemeral "at time" workers.
     *
     * Sets up the provided ephemeral worker with the bridging function. The worker
     * will be automatically removed from the context manager after execution.
     *
     * @param ctx Shared pointer to the context manager that will execute this worker
     * @param worker The ephemeral worker to use
     */
    EventBridge::EventBridge(const ContextManagerPtr& ctx, EphemeralWorker worker)
    : m_ephemeral_worker(std::move(worker)), m_ctx(ctx) {
        m_ephemeral_worker.setHandler(&ephemeral_bridging_function);
        m_ephemeral_worker.setPayload(this);
    }

    /**
     * @brief Destructor that handles cleanup based on worker type.
     *
     * For persistent "when pending" workers, deregisters the worker from the context manager.
     * For ephemeral "at time" workers with self-ownership, no action is needed as they are
     * automatically removed by the async context after execution.
     */
    EventBridge::~EventBridge() {

        if (m_self == nullptr) {
            // Check if m_worker was explicitly initialized (indicating a persistent worker)
            if (m_worker.getWorker()->do_work != nullptr) {
                m_ctx->removeWorker(m_worker);
            }
            // Otherwise, m_worker is in its default state, so this was an ephemeral worker
        }
    }

    /**
     * @brief Marks the worker as having pending work to be executed.
     *
     * This method adds the worker to the async context's FIFO queue for execution.
     * The worker will be executed when the async context processes its queue,
     * maintaining proper core affinity.
     */
    void EventBridge::run() {
        m_ctx->setWorkPending(m_worker);
    }

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
    void EventBridge::run(const uint32_t run_in) {
        DEBUGV("[c%d][%llu][INFO] EventBridge::run\n", rp2040.cpuid(), time_us_64());
        if (const auto result = m_ctx->addEphemeralWorker(m_ephemeral_worker, run_in); false == result) {

                DEBUGV("[c%d][%llu][ERROR] EventBridge::run - Failed to add ephemeral worker: %p, error: %lu\n",
                    rp2040.cpuid(), time_us_64(), this, result);

            // Infinite busy-wait loop for debugging:
            while (true) {
                e5::LedDebugger::setState(e5::LedDebugger::NONE_BRooo);
                m_ctx->waitUntil(make_timeout_time_us(10000));
            }
        }
    }

    /**
     * @brief Executes the work by calling the virtual onWork method.
     *
     * This method is called by the bridging function when the worker is executed.
     * It cannot be overridden by derived classes, ensuring consistent execution flow.
     */
    void EventBridge::doWork() {
        DEBUGV("[c%d][%llu][INFO] EventBridge::doWork\n", rp2040.cpuid(), time_us_64());
        onWork();
    }

    /**
     * @brief Takes ownership of self, enabling self-managed lifecycle.
     *
     * This method is typically used with ephemeral workers to allow them to manage
     * their own lifecycle. When an ephemeral worker takes ownership of itself,
     * it will be automatically destroyed after its execution completes.
     *
     * @param self A unique pointer to this EventBridge instance
     */
    void EventBridge::takeOwnership(std::unique_ptr<EventBridge> self) {
        m_self = std::move(self);
    }

    /**
     * @brief Releases ownership of self, transferring lifecycle management.
     *
     * This method is typically called in the bridging function to transfer ownership
     * to a local variable, which will destroy the EventBridge when it goes out of scope.
     *
     * @return A unique pointer to this EventBridge instance
     */
    std::unique_ptr<EventBridge> EventBridge::releaseOwnership() {
        return std::move(m_self);
    }


    /**
     * @brief Bridging function that connects the C-style callback to the C++ object.
     *
     * This function is called by the async context when a persistent worker is pending execution.
     * It retrieves the EventBridge instance from the worker's user_data and calls its doWork method.
     * The extern "C" linkage ensures correct calling convention for the C-based SDK.
     *
     * @param context The async context that is executing the worker
     * @param worker The async_when_pending_worker_t that is being executed
     */
    void worker_bridging_function(async_context_t* context, async_when_pending_worker_t* worker) {
        (void)context; // Unused parameter
        DEBUGV("[c%d][%llu][INFO] EventBridge::worker_bridging_function running on core: %d\n",
            rp2040.cpuid(), time_us_64(), context->core_num);

        if (worker && worker->user_data) {
            // Direct cast to EventBridge* since we're storing the EventBridge instance directly
            static_cast<EventBridge*>(worker->user_data)->doWork();
        } else {
            DEBUGV("[c%d][%llu][ERROR] SyncBridge::worker_bridging_function - invalid worker or user data\n",
                rp2040.cpuid(), time_us_64());
        }
    }

    /**
     * @brief Bridging function that connects the C-style callback to the C++ object for ephemeral workers.
     *
     * This function is called by the async context when an ephemeral worker's time arrives.
     * It retrieves the EventBridge instance from the worker's user_data, calls its doWork method,
     * and then releases ownership to trigger cleanup when the function returns.
     *
     * Note: By the time this function is called, the async context has already removed the
     * worker from its internal list, so no explicit removal is needed.
     *
     * @param context The async context that is executing the worker
     * @param worker The async_work_on_timeout that is being executed
     */
    void ephemeral_bridging_function(async_context_t* context, async_work_on_timeout* worker) {
        (void)context; // Unused parameter

        // Check if it's time to sample
        if (e5::g_queueMonitor.shouldSample()) {
            // Get worker counts
            const auto atTimeCount = e5::WorkerCounter::countAtTimeWorkers(context);
            const auto whenPendingCount = e5::WorkerCounter::countWhenPendingWorkers(context);

            // Update queue monitor
            e5::g_queueMonitor.updateQueueSize(atTimeCount, whenPendingCount);
        }

        if (worker && worker->user_data) {
            const auto pWorker = static_cast<EventBridge*>(worker->user_data);
            // Direct cast to EventBridge* since we're storing the EventBridge instance directly
            const auto local_owner = pWorker->releaseOwnership();
            local_owner->doWork();
        } else {
            DEBUGV("[c%d][%llu][ERROR] EventBridge::ephemeral_bridging_function - invalid worker or user data\n",
                rp2040.cpuid(), time_us_64());
            // intentional debug stopping
            while (true) {
                e5::LedDebugger::setState(e5::LedDebugger::NONE_oooGL);
                async_context_wait_until(context, make_timeout_time_us(100000));
            }
        }
    }


}