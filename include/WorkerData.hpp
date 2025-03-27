// WorkerData.hpp

#pragma once

namespace AsyncTcp {
    // Forward declaration
    class AsyncTcpClient;
    /**
     * @struct WorkerData
     * @brief Contains data necessary for a worker's processing within the asynchronous TCP client context.
     *
     * `WorkerData` holds essential information, such as the size of incoming data
     * and a reference to the associated TCP client, enabling worker functions to handle
     * and process network events effectively.
     */
    struct WorkerData
    {
        virtual ~WorkerData() = default;
        // Derived classes should implement this interface to handle specific data types.
        WorkerData() = default;
    };

} // namespace AsyncTcp
