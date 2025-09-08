/**
 * @file TcpWriter.hpp
 * @brief Header for stateful asynchronous TCP writer with chunking
 *
 * this TcpWriter maintains state to handle multi-chunk write operations with
 * proper ACK-driven flow control.
 *
 * Current semantics (incremental design step):
 *  - Progress variable (m_written) is advanced ONLY when bytes are ACKed
 * (tcp_sent).
 *  - A write operation is considered complete when ALL bytes are ACKed.
 *  - This is intentionally stronger than boost::asio::write() (which only
 *    guarantees local enqueue) and helps on RAM constrained targets (RP2040 +
 * lwIP).
 *  - Future plan: introduce separate counters (queued vs ACKed) and allow a
 *    policy selection (complete-on-enqueue vs complete-on-ACK).
 */

#pragma once

#include "ContextManager.hpp"
#include <Arduino.h>
#include <atomic>
#include <lwip/err.h>
#include <memory>

namespace async_tcp {

    class TcpClient; ///< Forward declaration of TcpClient class

    /**
     * @class TcpWriter
     * @brief Manages stateful asynchronous TCP write operations with chunking
     * @ingroup AsyncTCPClient
     *
     * This class holds the original data and manages multi-chunk write
     * operations. It creates ephemeral TcpWriteHandler instances for individual
     * chunks and handles ACK-driven continuation for large writes that don't
     * fit in a single TCP send buffer.
     */
    class TcpWriter final {
            static constexpr uint64_t STALL_TIMEOUT_US =
                2000000; ///< Stall timeout: no progress (queue or ACK) for this
                         ///< many microseconds
            // Completion policy: Acked (default) or Enqueued (complete when
            // fully queued regardless of ACKs)
            enum class CompletionMode : uint8_t { Acked = 0, Enqueued = 1 };
            // Watermark percentages applied to (cached_free + in-flight).
            static constexpr uint8_t HIGH_WATERMARK_PCT =
                70; // engage backpressure
            static constexpr uint8_t LOW_WATERMARK_PCT =
                50; // release backpressure
            static_assert(HIGH_WATERMARK_PCT > LOW_WATERMARK_PCT,
                          "Invalid watermark percentages");
            // Limit how many fragments we queue per sendNextChunk invocation
            // (coalescing strategy)
            static constexpr size_t MAX_FRAGMENTS_PER_CALL =
                1; // increase later if beneficial

            const AsyncCtx &m_ctx; ///< Context manager for worker execution
            TcpClient &m_io;       ///< TCP client for write operations

            // State for managing multi-chunk writes
            std::unique_ptr<uint8_t[]>
                m_data{};        ///< Original binary data being written
            size_t m_acked;      ///< Bytes successfully ACKed so far
            size_t m_queued;     ///< Bytes queued for sending (>= m_acked);
                                 ///< instrumentation only for now
            size_t m_total_size; ///< Total size of complete write operation
            std::atomic<bool>
                m_write_in_progress; ///< Thread-safe flag to prevent concurrent write operations
            size_t m_last_free_estimate =
                0; ///< Cached tcp_sndbuf snapshot (updated only inside async
                   ///< context) writes
            absolute_time_t
                m_write_start_time; ///< Timestamp when write operation started
            absolute_time_t m_last_progress_time =
                nil_time; ///< Last time we made progress (queued or ACKed
                          ///< bytes)
            CompletionMode m_mode =
                CompletionMode::Acked; ///< Current completion policy
            /**
             * @brief Send the next chunk of data
             * Internal method to handle chunked transmission.
             */
            void sendNextChunk(); // Attempts to send remaining bytes in one
                                  // chunk (mutates queued counter)

            /**
             * @brief Complete the write operation and cleanup state
             */
            void completeWrite();
            [[nodiscard]] size_t
            totalCapacityEstimate() const; // cached_free + in-flight (no direct
                                           // pcb access outside async ctx)
            [[nodiscard]] size_t
            dynamicHighWatermarkBytes() const; // derived from cached capacity
            [[nodiscard]] size_t
            dynamicLowWatermarkBytes() const; // derived from cached capacity
            // TODO(next incremental step): add m_queued counter and dual
            // completion policy
            size_t remainingUnqueued() const {
                return (m_total_size > m_queued) ? (m_total_size - m_queued)
                                                 : 0;
            }

        public:
            /**
             * @brief Constructor for TcpWriter
             * @param ctx Reference to Context manager for scheduling write
             * operations
             * @param io Reference to TcpClient for actual I/O operations
             */
            TcpWriter(const AsyncCtx &ctx, TcpClient &io);

            /**
             * @brief Destructor
             */
            ~TcpWriter() = default;

            /**
             * @brief Start an asynchronous write operation
             * @param data Pointer to binary data to write
             * @param size Size of the data to write
             */
            void write(const uint8_t *data, size_t size);

            void setCompletionMode(CompletionMode mode) { m_mode = mode; }
            [[nodiscard]] CompletionMode getCompletionMode() const {
                return m_mode;
            }
            // Convenience wrappers
            void enableEnqueueComplete() { m_mode = CompletionMode::Enqueued; }
            void enableAckComplete() { m_mode = CompletionMode::Acked; }
            [[nodiscard]] bool isEnqueueCompleteMode() const {
                return m_mode == CompletionMode::Enqueued;
            }
            [[nodiscard]] bool isAckCompleteMode() const {
                return m_mode == CompletionMode::Acked;
            }

            /**
             * @brief Check if a write operation is in progress
             * @return true if write is in progress, false otherwise
             */
            [[nodiscard]] bool isWriteInProgress() const {
                return m_write_in_progress.load();
            }

            /**
             * @brief Atomically try to start a write operation
             * Uses compare_exchange_strong to atomically check if no write is
             * in progress and set it to true if so, eliminating race
             * conditions.
             * @param expected Reference to expected value (should be false)
             * @return true if write was successfully started, false if already
             * in progress.
             */
            bool tryStartWrite(bool &expected) {
                return m_write_in_progress.compare_exchange_strong(expected,
                                                                   true);
            }

            /**
             * @brief Handle ACK notification to continue multi-chunk writes
             * Called by TcpClient when ACK is received from remote peer.
             */
            void onAckReceived(uint16_t ack_len);

            void onError(err_t error);

            /**
             * @brief Check if the current write operation has timed out
             * @return true if write has timed out, false otherwise
             */
            [[nodiscard]] bool hasTimedOut() const; // Stall timeout check

            /**
             * @brief Handle write timeout — called when timeout is detected
             * Cleans up the write operation and notifies about a timeout.
             */
            void onWriteTimeout();

            // Accessors (diagnostics / future policy work)
            [[nodiscard]] size_t ackedBytes() const { return m_acked; }
            [[nodiscard]] size_t queuedBytes() const { return m_queued; }
            [[nodiscard]] size_t totalBytes() const { return m_total_size; }
            [[nodiscard]] size_t inFlightBytes() const {
                return (m_queued >= m_acked) ? (m_queued - m_acked) : 0;
            }
            [[nodiscard]] bool
            shouldBackpressure() const; // Uses cached free estimate
            [[nodiscard]] bool
            canReleaseBackpressure() const; // Uses cached free estimate
    };

} // namespace async_tcp
