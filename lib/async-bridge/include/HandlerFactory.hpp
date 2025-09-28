/**
 * @file HandlerFactory.hpp
 * @brief A centralized factory for creating ephemeral handlers
 *
 * This singleton class provides a central point for creating ephemeral handlers
 * while decoupling components from direct AsyncCtx dependencies. It implements
 * a factory pattern that allows any component to create handlers without
 * needing to know about the AsyncCtx details.
 */

#pragma once
#include "ContextManager.hpp"

#include <utility>
#include <type_traits>

namespace async_tcp {

    // Forward declaration
    class EphemeralBridge;

    /**
     * @class HandlerFactory
     * @brief Singleton factory for creating and running ephemeral handlers
     *
     * This class implements a singleton pattern to provide centralized access
     * to the AsyncCtx required by all handlers. It eliminates the need to pass
     * AsyncCtx throughout the application by providing a templated factory
     * method that forwards the appropriate parameters to any handler type.
     */
    class HandlerFactory {
            AsyncCtx &m_ctx; /**< Reference to the async context */
            inline static HandlerFactory *s_instance =
                nullptr; /**< Singleton instance pointer */

            /**
             * @brief Constructs the HandlerFactory with the given AsyncCtx
             *
             * @param ctx Reference to the AsyncCtx used by all handlers
             */
            explicit HandlerFactory(AsyncCtx &ctx) : m_ctx(ctx) {}

        public:
            /**
             * @brief Initializes the singleton instance
             *
             * Must be called once during application startup before any
             * handlers are created.
             *
             * @param ctx Reference to the AsyncCtx used by all handlers
             */
            static void initialise(AsyncCtx &ctx) {
                s_instance = new HandlerFactory(ctx);
            }

            /**
             * @brief Gets the singleton instance
             *
             * @return Reference to the singleton HandlerFactory instance
             * @throws assertion failure if called before initialise()
             */
            static HandlerFactory &instance() {
                assert(s_instance && "HandlerFactory must be initialised before use");
                return *s_instance;
            }

            /**
             * @brief Creates and runs an ephemeral handler
             *
             * This templated method creates a handler of the specified type,
             * forwards the provided arguments to its create() method, and
             * schedules it for execution on the async context.
             *
             * @tparam HandlerType The handler type to create (must derive from EphemeralBridge)
             * @tparam Args Variadic template for handler constructor arguments
             * @param args Arguments to forward to the handler's create() method
             */
            template<typename HandlerType, typename... Args>
            void run(Args&&... args) {
                static_assert(std::is_base_of_v<EphemeralBridge, HandlerType>, 
                             "HandlerType must derive from EphemeralBridge");
                HandlerType::create(m_ctx, std::forward<Args>(args)...);
            }
    };

} // namespace async_tcp
