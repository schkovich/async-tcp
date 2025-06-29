//
// Created by goran on 02/03/25.
//

#pragma once

#include <pico/async_context_threadsafe_background.h>

namespace async_tcp {

    class WorkerBase {

        public:
            /**
             * Default destructor for `WorkerBase`.
             */
            virtual ~WorkerBase() = default;

            /**
             *
             * @param data raw pointer
             */
            virtual void setPayload(void *data) = 0;
    };

} // namespace AsyncTcp
