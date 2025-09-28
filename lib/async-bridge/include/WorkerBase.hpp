// cpp
// SPDX-License-Identifier: MPL-2.0
#pragma once
#include <type_traits>
#include <utility>

namespace async_bridge {

    // ---------- traits primary (no `type`) ----------
    template <class T>
    struct handler_traits; // primary left intentionally undefined

    // detect presence of user_data member
    template <class T, class = void> struct has_user_data : std::false_type {};
    template <class T>
    struct has_user_data<T, std::void_t<decltype(std::declval<T>().user_data)>>
        : std::true_type {};

    // detect presence of do_work member
    template <class T, class = void> struct has_do_work : std::false_type {};
    template <class T>
    struct has_do_work<T, std::void_t<decltype(std::declval<T>().do_work)>>
        : std::true_type {};

    template <class Derived, class WorkerT> class WorkerBase {
        protected:
            // base-owned worker instance
            WorkerT m_worker{};

            template <typename... Args>
            auto callHandler(Args &&...args)
                -> std::invoke_result_t<typename handler_traits<Derived>::type,
                                        Args...> {
                using result_t = std::invoke_result_t<raw_handler_t, Args...>;

                auto f = m_worker.do_work; // function pointer stored in the
                                           // worker struct
                if constexpr (std::is_void_v<result_t>) {
                    if (f)
                        f(std::forward<Args>(args)...);
                    return;
                } else {
                    if (f)
                        return f(std::forward<Args>(args)...);
                    return result_t{};
                }
            }

        public:
            using raw_handler_t = typename handler_traits<Derived>::type;

            static_assert(
                std::is_pointer_v<raw_handler_t>,
                "handler_traits<Derived>::type must be a pointer to function");
            static_assert(
                std::is_function_v<std::remove_pointer_t<raw_handler_t>>,
                "handler_traits<Derived>::type must point to a function type");
            static_assert(has_user_data<WorkerT>::value,
                          "WorkerT must have a `user_data` member");
            static_assert(has_do_work<WorkerT>::value,
                          "WorkerT must have a `do_work` member");
            static_assert(
                std::is_same_v<raw_handler_t,
                               decltype(std::declval<WorkerT>().do_work)>,
                "handler_traits<Derived>::type must match WorkerT::do_work "
                "type");

            WorkerBase() = default;
            virtual ~WorkerBase() = default;

            // handler API now writes into the worker's do_work field.
            void setHandler(raw_handler_t h) noexcept { m_worker.do_work = h; }
            void clearHandler() noexcept { m_worker.do_work = nullptr; }

            // payload setter is non-virtual and operates on the base-owned
            // worker
            void setPayload(void *data) noexcept { m_worker.user_data = data; }
    };

} // namespace async_bridge
