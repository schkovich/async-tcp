// SPDX-License-Identifier: MPL-2.0
#pragma once

#include "EventBridge.hpp"

#include <memory>

namespace async_bridge {

class PerpetualWorker; // forward-declared handle

class PerpetualBridge : public EventBridge {
public:
    explicit PerpetualBridge(const IAsyncContext &ctx) : EventBridge(ctx) {}

    // Implementations should remove worker in the destructor via context
    ~PerpetualBridge() override = default;

    void initialiseBridge() override;

    void run();

    // Optional workload entry point for derived classes
    virtual void workload(void *data) { (void)data; }

protected:
    PerpetualWorker *m_perpetual_worker = nullptr;
};

using PerpetualBridgePtr = std::unique_ptr<PerpetualBridge>;

} // namespace async_bridge

