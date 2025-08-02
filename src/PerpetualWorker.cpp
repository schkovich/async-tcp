//
// Created by goran on 02/03/25.
//

#include "PerpetualWorker.hpp"

namespace async_tcp {

    PerpetualWorker::PerpetualWorker() : m_worker{} {}

    async_when_pending_worker_t *PerpetualWorker::getWorker() {
        return &m_worker;
    }

    void PerpetualWorker::setHandler(void (*handler_func)(
        async_context_t *, async_when_pending_worker_t *)) {
        m_worker.do_work = handler_func;
    }

    void PerpetualWorker::setPayload(void *data) { m_worker.user_data = data; }

} // namespace async_tcp