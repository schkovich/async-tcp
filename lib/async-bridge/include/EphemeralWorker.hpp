// SPDX-License-Identifier: MPL-2.0
#pragma once

#include "IAsyncContext.hpp"
#include "WorkerBase.hpp"

namespace async_bridge {
    class EphemeralWorker;
    template <> struct handler_traits<EphemeralWorker> {
            using type = ephemeral_bridging_function_t;
    };
    class EphemeralWorker final : public WorkerBase<EphemeralWorker, ephemeral_worker_t> {

        public:
            EphemeralWorker() = default;
            ~EphemeralWorker() override = default;
    };

} // namespace async_bridge
