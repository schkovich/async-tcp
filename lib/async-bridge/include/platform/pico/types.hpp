//
// Created by goran on 28/09/2025.
//

#pragma once
#include <cstdint>
#include <pico/async_context.h>

namespace async_bridge {
    /**
     * @typedef uint8_t byte
     * @brief Defines a byte as an unsigned 8-bit integer.
     *
     * This type alias improves code readability by providing a clear
     * representation of a byte, which is commonly used in low-level data
     * manipulation and communication protocols.
     */
    using byte = std::uint8_t;

    using handler_function_t = uint32_t (*)(void *param);

    using perpetual_bridging_function_t =
        void (*)(async_context_t *, async_when_pending_worker_t *);

    using ephemeral_bridging_function_t =
        void (*)(async_context_t *,
                                  async_work_on_timeout *);

    using perpetual_worker_t = async_when_pending_worker_t;
        using ephemeral_worker_t = async_at_time_worker_t;
} // namespace async_bridge