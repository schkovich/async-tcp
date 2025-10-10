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


#include <Arduino.h>
#include <cstring>
#include <functional>
#include <lwip/err.h>
#include <lwip/tcp.h>
#include <memory>

namespace async_tcp {

    class TcpClient;

    extern "C" err_t lwip_sent_cb(void *arg, tcp_pcb *tpcb,
                                  u16_t len); // pure C ACK bridge

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

            using AckCallback = std::function<void(tcp_pcb *, std::size_t)>;

            tcp_pcb *m_pcb = nullptr; ///< Pointer to the TCP PCB
            friend err_t lwip_sent_cb(void *arg, tcp_pcb *tpcb, u16_t len);
            static constexpr uint64_t STALL_TIMEOUT_US =
                2000000; ///< Stall timeout: no progress (queue or ACK) for this
                         ///< many microseconds.
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

            // State for managing multi-chunk writes
            std::unique_ptr<uint8_t[]>
                m_data{};            ///< Original binary data being written
            std::size_t m_acked{0};  ///< Bytes successfully ACKed so far
            std::size_t m_queued{0}; ///< Bytes queued for sending (>= m_acked);
                                     ///< instrumentation only for now
            std::size_t m_total_size{
                0}; ///< Total size of complete write operation
            absolute_time_t m_write_start_time{
                nil_time}; ///< Timestamp when write operation started
            absolute_time_t m_last_progress_time =
                nil_time; ///< Last time we made progress (queued or ACKed
                          ///< bytes)
            CompletionMode m_mode =
                CompletionMode::Acked; ///< Current completion policy

            AckCallback m_ack_cb; // optional external ACK observer

            /**
             * @brief Determine the size of the next chunk to send. Uses the
             * smaller of remaining data and available send buffer space.
             * @param remaining Remaining bytes to send
             * @param send_buffer_free Current free space in the TCP send buffer
             * @return Size of the next chunk to send
             */
            [[nodiscard]] std::size_t
            getChunkSize(const std::size_t remaining,
                         const std::size_t send_buffer_free) const {
                return std::min(std::min(remaining, send_buffer_free),
                                static_cast<std::size_t>(TCP_MSS));
            }

            /**
             * @brief Get pointer to the data for the next chunk to send.
             * @param size Current offset into the data buffer
             * @return Pointer to the data for the next chunk
             */
            [[nodiscard]]
            const unsigned char *getChunkData(const std::size_t size) {
                return m_data.get() + size;
            }

            [[nodiscard]] std::size_t availableForWrite() const;


        public:
            /**
             * @brief Constructor for TcpWriter
             * @param pcb Reference to PCB for scheduling write operations
             */
            explicit TcpWriter(tcp_pcb *pcb);

            /**
             * @brief Destructor
             */
            ~TcpWriter() = default;

            /**
             * @brief Write data directly to TCP without buffer management
             * @param data Pointer to data buffer (owned by caller)
             * @param size Size of data to write
             * @return Number of bytes successfully queued
             */
            std::size_t writeData(const uint8_t *data, std::size_t size);

            /**
             * @brief Get optimal chunk size for current send buffer state
             * @param data_size Size of data wanting to send
             * @return Optimal chunk size respecting TCP_MSS and send buffer
             */
            [[nodiscard]] std::size_t
            getOptimalChunkSize(const std::size_t data_size) const {
                const auto free_buffer = availableForWrite();
                return std::min(std::min(data_size, free_buffer),
                                static_cast<std::size_t>(TCP_MSS));
            }

            /**
             * @brief Check if send buffer has space for writing
             * @return true if send buffer has space, false otherwise
             */
            [[nodiscard]] bool canWriteNow() const {
                return availableForWrite() > 0;
            }

            void onAckCallback(tcp_pcb *pcb, uint16_t len);

            void setOnAckCallback(const AckCallback &cb) { m_ack_cb = cb; }

            void onError(err_t error);
    };

} // namespace async_tcp
