/**
 * @file TcpWriteHandler.hpp
 * @brief Defines a handler for TCP write operations with proper serialization.
 *
 * This file contains the TcpWriteHandler class which implements the
 * EventBridge pattern to handle TCP write operations in a thread-safe manner.
 * The handler is ephemeral and handles a single chunk write operation.
 *
 * The handler is designed to be simple and stateless - it receives only
 * the data it needs to write and self-destructs after execution.
 *
 * @author Goran
 * @date 2025-02-20
 * @ingroup AsyncTCPClient
 */

#pragma once
#include "ContextManager.hpp"
#include "EventBridge.hpp"
#include "TcpClient.hpp"
#include <functional>
#include <memory>

namespace async_tcp {

    /**
     * @class TcpWriteHandler
     * @brief Handles single TCP chunk write operations using EventBridge pattern
     *
     * This handler is ephemeral and designed for simple, one-shot TCP write
     * operations. It receives only the data chunk it needs to write and
     * self-destructs after execution. The async_context provides the necessary
     * serialization guarantees.
     */
    class TcpWriteHandler final : public EventBridge {
        private:
            TcpClient& m_io;                      ///< TCP client for write operations
            const uint8_t* m_data;                ///< Pointer to binary data chunk to write
            size_t m_size;                   ///< Remaining bytes to writ

        protected:
            /**
             * @brief Performs the TCP write operation
             *
             * This method writes the single data chunk using TcpClient::writeChunk().
             * From this handler's perspective, it always writes from offset 0.
             */
            void onWork() override;

        public:
            /**
             * @brief Constructs a TcpWriteHandler for a single chunk write.
             *
             * @param ctx Context manager that will execute this handler on Core 0
             * @param data Pointer to binary data chunk to write
             * @param size Size of data chunk to write
             * @param io Reference to the TCP client that will perform the write
             */
            TcpWriteHandler(const AsyncCtx &ctx,
                           const uint8_t* data,
                           size_t size,
                           TcpClient& io);

            /**
             * @brief Factory method to create and execute TcpWriteHandler
             *
             * Following the PrintHandler pattern, this static method creates
             * the handler and immediately schedules it for execution.
             *
             * @param ctx Context manager for execution
             * @param data Pointer to binary data chunk to write
             * @param size Size of data chunk to write
             * @param io TCP client to use
             */
            static void create(const AsyncCtx & ctx,
                               const uint8_t* data,
                               size_t size,
                               TcpClient& io) {
                // Assert that the calling core matches the context core
                assert(get_core_num() == ctx.getCore());
                auto handler = std::make_unique<TcpWriteHandler>(ctx, data, size, io);
                TcpWriteHandler* raw_ptr = handler.get();
                raw_ptr->initialiseEphemeralBridge();
                raw_ptr->takeOwnership(std::move(handler));
                raw_ptr->run(0);
            }
    };

} // namespace async_tcp
