#include "EphemeralBridge.hpp"

namespace async_tcp {

    // ReSharper disable once CppParameterMayBeConstPtrOrRef
    void ephemeral_bridging_function(
        async_context_t *context,
        // ReSharper disable once CppParameterMayBeConstPtrOrRef
        async_work_on_timeout *worker) {
        (void)context;
        assert(worker && worker->user_data);
        const auto local_bridge =
            static_cast<EphemeralBridge *>(worker->user_data)
                ->releaseOwnership();
        worker->user_data = nullptr;
        local_bridge->doWork();
    }

    std::unique_ptr<EphemeralBridge> EphemeralBridge::releaseOwnership() {
        return std::move(m_self);
    }

    void EphemeralBridge::initialiseBridge() {
        m_ephemeral_worker.setHandler(&ephemeral_bridging_function);
        m_ephemeral_worker.setPayload(this);
    }

    void EphemeralBridge::takeOwnership(std::unique_ptr<EphemeralBridge> self) {
        m_self = std::move(self);
    }

    void EphemeralBridge::run(const uint32_t run_in) {
        if (const auto result =
                getContext().addWorker(m_ephemeral_worker, run_in);
            !result) {
            DEBUGCORE("[c%d][%llu][ERROR] EphemeralBridge::run - Failed to "
                      "add ephemeral worker: %p, error: %lu\n",
                      rp2040.cpuid(), time_us_64(), this, result);
        }
    }

} // namespace async_tcp
