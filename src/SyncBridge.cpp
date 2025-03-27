//
// Created by goran on 21/02/25.
//

#include "SyncBridge.hpp"
#include "Arduino.h"
#include "e5/LedDebugger.hpp"

namespace AsyncTcp {

    uint32_t SyncBridge::doExecute() {
        DEBUGV("[c%d][%llu][INFO] SyncBridge::doExecute()\n", rp2040.cpuid(), time_us_64());
        const auto result = onExecute(std::move(m_executor_payload));
        return result;
    }

    /**
     * busy-waits to acquire a lock on the context
     */
    void SyncBridge::lockBridge() const { m_ctx->acquireLock(); }

    void SyncBridge::unlockBridge() const { m_ctx->releaseLock(); }

    void SyncBridge::pause(const absolute_time_t until) const {
        m_ctx->waitUntil(until);
    }

    uint32_t SyncBridge::execute(std::unique_ptr<SyncPayload> payload) {
        if (e5::LedDebugger::getState() == e5::LedDebugger::RIGHT_ooooo) {
            e5::LedDebugger::setState(e5::LedDebugger::RIGHT_BRYGL);
            DEBUGV("[c%d][%llu][INFO] SyncBridge::execute()\n", rp2040.cpuid(), time_us_64());
            m_executor_payload = std::move(payload);
            const auto result = m_ctx->execWorkSynchronously(&executor_bridging_function, this);
            e5::LedDebugger::setState(e5::LedDebugger::RIGHT_ooooo);
            return result;
        }
        return -99;
    }

    /**
     * @brief A C-style bridging function that delegates to SyncExecutor for synchronous execution.
     */
    uint32_t executor_bridging_function(void* instance) {
        DEBUGV("[c%d][%llu][INFO] SyncBridge::executor_bridging_function\n", rp2040.cpuid(), time_us_64());
        auto* executor = static_cast<SyncBridge*>(instance);
        const auto result = executor->doExecute();
        // executor->pause(make_timeout_time_us(10000)); // Wait for 1 second
        return result;
    }

} // namespace AsyncTcp
