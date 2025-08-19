// EphemeralWorker.cpp

#include "EphemeralWorker.hpp"

namespace async_tcp {

    EphemeralWorker::EphemeralWorker() : m_worker{nullptr} {}

    void EphemeralWorker::setHandler(
        void (*handler_function)(async_context_t *, async_work_on_timeout *)) {
        m_worker.do_work = handler_function;
    }

    async_at_time_worker_t *EphemeralWorker::getWorker() { return &m_worker; }

    void EphemeralWorker::setPayload(void *data) { m_worker.user_data = data; }

} // namespace async_tcp
