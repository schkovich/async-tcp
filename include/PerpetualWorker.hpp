//
// Created by goran on 02/03/25.
//

#pragma once
#include "WorkerBase.hpp"

namespace AsyncTcp {

class PerpetualWorker final : public WorkerBase {
    async_when_pending_worker_t m_worker;            /**< Internal worker instance for async processing. */

public:

    PerpetualWorker();

    virtual ~PerpetualWorker();

    /**
     * @brief Retrieves the internal worker instance.
     *
     * @return Pointer to the `async_when_pending_worker_t` worker instance.
     *
     * This function provides access to the internal worker instance for use in
     * managing and monitoring asynchronous work states. Primarily intended for internal use.
     */
    async_when_pending_worker_t *getWorker();

};

} // AsyncTcp
