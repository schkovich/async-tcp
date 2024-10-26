// WorkerData.hpp

#pragma once
#include <memory>

namespace AsyncTcp {
    class AsyncTcpClient;  // Forward declaration

    /**
     * @struct WorkerData
     * @brief Contains data necessary for a worker's processing within the asynchronous TCP client context.
     *
     * `WorkerData` holds essential information, such as the size of incoming data
     * and a reference to the associated TCP client, enabling worker functions to handle
     * and process network events effectively.
     */
    struct WorkerData {
        std::unique_ptr<int> read_size;      /**< Pointer to the size of data available for reading. */
        AsyncTcpClient* client = nullptr;    /**< Raw pointer to the associated `AsyncTcpClient` instance. */

        // Additional fields can be added here as needed to support more complex data handling.
    };

} // namespace AsyncTcp
