// SPDX-License-Identifier: MPL-2.0
#pragma once

#include "IAsyncContext.hpp"
#include "WorkerBase.hpp"

namespace async_bridge {
    class PerpetualWorker;
    template <> struct handler_traits<PerpetualWorker> {
            using type = perpetual_bridging_function_t;
    };

    class PerpetualWorker final : public WorkerBase<PerpetualWorker, perpetual_worker_t> {

        public:
            PerpetualWorker() = default;
            ~PerpetualWorker() override = default;
    };

} // namespace async_bridge
