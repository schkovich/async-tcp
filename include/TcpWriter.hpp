/**
 * @file TcpWriter.hpp
 * @brief Header for stateful asynchronous TCP writer with chunking
 *
 * this TcpWriter maintains state to handle multi-chunk write operations with
 * proper ACK-driven flow control.
 *
 * @author Goran
 * @date 2025-02-20
 */

#pragma once

#include "ContextManager.hpp"

#include <Arduino.h>
#include <atomic>
#include <lwip/err.h>
#include <memory>

namespace async_tcp {

    class TcpClient;  ///< Forward declaration of TcpClient class

    /**
     * @class TcpWriter
     * @brief Manages stateful asynchronous TCP write operations with chunking
     *
     * This class holds the original data and manages multi-chunk write operations.
     * It creates ephemeral TcpWriteHandler instances for individual chunks and
     * handles ACK-driven continuation for large writes that don't fit in a single
     * TCP send buffer.
     */
    class TcpWriter final {

            static constexpr uint64_t WRITE_TIMEOUT_US = 5000000;  ///< Write timeout in microseconds (5 seconds)

            const AsyncCtx & m_ctx;           ///< Context manager for worker execution
            TcpClient& m_io;                          ///< TCP client for write operations

            // State for managing multi-chunk writes
            std::unique_ptr<uint8_t[]> m_data;        ///< Original binary data being written
            size_t m_written;                         ///< Bytes successfully written so far
            size_t m_total_size;                      ///< Total size of complete write operation
            std::atomic<bool> m_write_in_progress;    ///< Thread-safe flag to prevent concurrent writes
            absolute_time_t m_write_start_time;       ///< Timestamp when write operation started

        public:
            /**
             * @brief Constructor for TcpWriter
             * @param ctx Reference to Context manager for scheduling write operations
             * @param io Reference to TcpClient for actual I/O operations
             */
            TcpWriter(const AsyncCtx & ctx, TcpClient& io);

            /**
             * @brief Destructor
             */
            ~TcpWriter() = default;

            /**
             * @brief Start an asynchronous write operation
             * @param data Pointer to binary data to write
             * @param size Size of the data to write
             */
            void write(const uint8_t* data, size_t size);

            /**
             * @brief Check if a write operation is currently in progress
             * @return true if write is in progress, false otherwise
             */
            [[nodiscard]] bool isWriteInProgress() const { return m_write_in_progress.load(); }

            /**
             * @brief Atomically try to start a write operation
             * Uses compare_exchange_strong to atomically check if no write is in progress
             * and set it to true if so, eliminating race conditions.
             * @param expected Reference to expected value (should be false)
             * @return true if write was successfully started, false if already in progress
             */
            bool tryStartWrite(bool& expected) {
                return m_write_in_progress.compare_exchange_strong(expected, true);
            }

            /**
             * @brief Handle ACK notification to continue multi-chunk writes
             * Called by TcpClient when ACK is received from remote peer
             */
            void onAckReceived(uint16_t ack_len);

            void onError(err_t error);

            /**
             * @brief Check if the current write operation has timed out
             * @return true if write has timed out, false otherwise
             */
            [[nodiscard]] bool hasTimedOut() const;

            /**
             * @brief Handle write timeout - called when timeout is detected
             * Cleans up the write operation and notifies about timeout
             */
            void onWriteTimeout();

        private:
            /**
             * @brief Send the next chunk of data
             * Internal method to handle chunked transmission
             */
            void sendNextChunk();

            /**
             * @brief Complete the write operation and cleanup state
             */
            void completeWrite();
    };

} // namespace async_tcp
