// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#ifdef TERSE_ENABLE_SSE
    #define ENABLE_SSE_BSWAP
#endif  // TERSE_ENABLE_SSE

#include "terse/utils/Endianness.h"

#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable : 4365 4987)
#endif
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>
#ifdef _MSC_VER
    #pragma warning(pop)
#endif

namespace terse {

namespace traits {

template<std::size_t size>
struct uint_of_size {
    using type = typename std::conditional<(size == 1ul), std::uint8_t,
                                           typename std::conditional<(size == 2ul), std::uint16_t,
                                                                     typename std::conditional<(size <= 4ul), std::uint32_t,
                                                                                               std::uint64_t>::type>::type>::type;
};

}  // namespace traits

namespace impl {

struct block128 {

    static constexpr std::size_t alignment() {
        #if defined(__arm__) || defined(__aarch64__) || defined(_M_ARM) || defined(_M_ARM64)
            return std::alignment_of<std::max_align_t>::value;
        #else
            return 16ul;
        #endif
    }

};

}  // namespace impl

template<typename T>
inline void networkToHost(T& value) {
    using UIntType = typename traits::uint_of_size<sizeof(T)>::type;
    static_assert(sizeof(T) == sizeof(UIntType), "No matching unsigned integral type found for the given type.");
    // Using memcpy is the only well-defined way of reconstructing arbitrary types from raw bytes.
    // The seemingly unnecessary copies and memcpy calls are all optimized away,
    // compiler knows what's up.
    UIntType hostOrder;
    std::memcpy(&hostOrder, &value, sizeof(T));
    hostOrder = ntoh(hostOrder);
    std::memcpy(&value, &hostOrder, sizeof(T));
}

template<typename T>
inline void networkToHost128(T* values) {
    using UIntType = typename traits::uint_of_size<sizeof(T)>::type;
    static_assert(sizeof(T) == sizeof(UIntType), "No matching unsigned integral type found for the given type.");
    // Using memcpy is the only well-defined way of reconstructing arbitrary types from raw bytes.
    // The seemingly unnecessary copies and memcpy calls are all optimized away,
    // compiler knows what's up.
    alignas(impl::block128::alignment()) UIntType hostOrder[16ul / sizeof(T)];
    std::memcpy(static_cast<UIntType*>(hostOrder), values, 16ul);
    ntoh(static_cast<UIntType*>(hostOrder));
    std::memcpy(values, static_cast<UIntType*>(hostOrder), 16ul);
}

template<typename T>
inline void hostToNetwork(T& value) {
    using UIntType = typename traits::uint_of_size<sizeof(T)>::type;
    static_assert(sizeof(T) == sizeof(UIntType), "No matching unsigned integral type found for the given type.");
    // Using memcpy is the only well-defined way of reconstructing arbitrary types from raw bytes.
    // The seemingly unnecessary copies and memcpy calls are all optimized away,
    // compiler knows what's up.
    UIntType networkOrder;
    std::memcpy(&networkOrder, &value, sizeof(T));
    networkOrder = hton(networkOrder);
    std::memcpy(&value, &networkOrder, sizeof(T));
}

template<typename T>
inline void hostToNetwork128(T* values) {
    using UIntType = typename traits::uint_of_size<sizeof(T)>::type;
    static_assert(sizeof(T) == sizeof(UIntType), "No matching unsigned integral type found for the given type.");
    // Using memcpy is the only well-defined way of reconstructing arbitrary types from raw bytes.
    // The seemingly unnecessary copies and memcpy calls are all optimized away,
    // compiler knows what's up.
    alignas(impl::block128::alignment()) UIntType networkOrder[16ul / sizeof(T)];
    std::memcpy(static_cast<UIntType*>(networkOrder), values, 16ul);
    hton(static_cast<UIntType*>(networkOrder));
    std::memcpy(values, static_cast<UIntType*>(networkOrder), 16ul);
}

}  // namespace terse
