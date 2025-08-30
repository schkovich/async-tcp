// iprs_util.hpp
#pragma once

#if defined(__ARM_ARCH) || defined(__thumb__)
#include <cstdint>
static std::uint32_t read_ipsr() noexcept {
    uint32_t v;
    __asm volatile ("mrs %0, ipsr" : "=r"(v) );
    return v;
}
#else
static inline uint32_t read_ipsr() noexcept { return 0; }   // host fallback
#endif

static bool is_in_isr() noexcept { return read_ipsr() != 0; }