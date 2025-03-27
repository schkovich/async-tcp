// ReceiveCallbackHandler.cpp

#include "ReceiveCallbackHandler.hpp"
#include "../examples/tmp e5/WorkerDataTmp.hpp"
#include "AsyncTcpClient.hpp"

namespace AsyncTcp {

    /**
     * @brief Handles incoming data events by setting up necessary worker data.
     *
     * The `handleEvent` method allocates a new `WorkerData` instance, initializes
     * it with the amount of available data from the m_io, and associates it with the worker.
     * Finally, it signals the context that there is pending work to be processed.
     *
     * - Allocates a `WorkerData` instance for the incoming data.
     * - Sets the `read_size` member of `WorkerData` based on available data in `client`.
     * - Associates the `WorkerData` instance with `_worker`.
     * - Notifies `_ctx` of pending work for `_worker`.
     */
    void ReceiveCallbackHandler::handleEvent() {
        // auto data = std::make_unique<WorkerDataTmp>(m_io);  // Allocate WorkerData
        //
        // // Capture the available size of data from the m_io
        // auto size = std::make_unique<int>(m_io.available());
        //
        // data->read_size = std::move(size);       // Set read size in WorkerData
        //
        // // Pass WorkerData to the worker and notify context of pending work
        // _worker->setWorkerData(std::move(data));
        _ctx->setWorkPending(*_worker);
    }

} // namespace AsyncTcp
