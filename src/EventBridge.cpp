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

    EventBridge::EventBridge(const AsyncCtx &ctx) : m_ctx(ctx) {}

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

    void EventBridge::run() { m_ctx.setWorkPending(m_perpetual_worker); }

    void EventBridge::run(const uint32_t run_in) { // NOLINT
        if (const auto result = m_ctx.addWorker(m_ephemeral_worker, run_in);
            !result) {
            DEBUGV("[c%d][%llu][ERROR] EventBridge::run - Failed to add "
                   "ephemeral worker: %p, error: %lu\n",
                   rp2040.cpuid(), time_us_64(), this, result);
        }
    }

    void EventBridge::doWork() { onWork(); }

    void EventBridge::takeOwnership(std::unique_ptr<EventBridge> self) {
        m_self = std::move(self);
    }

    std::unique_ptr<EventBridge> EventBridge::releaseOwnership() {
        return std::move(m_self);
    }

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

} // namespace async_tcp
