// SPDX-License-Identifier: MPL-2.0
#pragma once

#include "IAsyncContext.hpp"

#include <type_traits>
#include <utility>

namespace async_bridge {

class EphemeralBridge;

/**
 * Minimal factory used to create ephemeral handlers. During migration this
 * can be replaced by a DI-friendly factory; for now a small singleton helper
 * is provided for convenience in tests and examples.
 */
class HandlerFactory {
public:
    explicit HandlerFactory(IAsyncContext &ctx) : m_ctx(ctx) {}

    template <typename HandlerType, typename... Args>
    void run(Args &&... args) {
        static_assert(std::is_base_of_v<EphemeralBridge, HandlerType>, "HandlerType must derive from EphemeralBridge");
        HandlerType::create(m_ctx, std::forward<Args>(args)...);
    }

private:
    IAsyncContext &m_ctx;
};

} // namespace async_bridge

