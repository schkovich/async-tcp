// SPDX-License-Identifier: MPL-2.0
#pragma once

#include "EventBridge.hpp"

#include <memory>

namespace async_bridge {

class EphemeralWorker; // forward

class EphemeralBridge : public EventBridge {
public:
    explicit EphemeralBridge(const IAsyncContext &ctx) : EventBridge(ctx) {}

    void initialiseBridge() override;

    void run(uint32_t run_in_ms = 0);

protected:
    // Self ownership helper for ephemeral lifetime management
    std::unique_ptr<EphemeralBridge> m_self = nullptr;

    void takeOwnership(std::unique_ptr<EphemeralBridge> self) { m_self = std::move(self); }
    std::unique_ptr<EphemeralBridge> releaseOwnership() { return std::move(m_self); }
};

} // namespace async_bridge

