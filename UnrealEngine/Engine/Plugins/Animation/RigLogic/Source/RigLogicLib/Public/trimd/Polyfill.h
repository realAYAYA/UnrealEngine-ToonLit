// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

// *INDENT-OFF*
#if defined(TRIMD_ENABLE_AVX) || defined(TRIMD_ENABLE_SSE)
#include <immintrin.h>

#include <cstdint>
#include <cstring>

#if (!defined(__clang__) && defined(_MSC_VER) && _MSC_VER <= 1900) || (!defined(__clang__) && defined(__GNUC__) && \
__GNUC__ < 9)
    #define _mm_loadu_si64 _mm_loadl_epi64
#endif

#if (!defined(__clang__) && defined(_MSC_VER) && _MSC_VER <= 1900) || \
(defined(__clang__) && __clang_major__ < 8) || \
(!defined(__clang__) && defined(__GNUC__) && __GNUC__ < 11)
    inline __m128i _mm_loadu_si16(const void* source) {
        return _mm_insert_epi16(_mm_setzero_si128(), *reinterpret_cast<const std::int16_t*>(source), 0);
    }
#endif

#if !defined(__clang__) && defined(_MSC_VER) && (_MSC_VER <= 1900) && defined(_WIN32) && !defined(_WIN64)
inline std::int64_t _mm_cvtsi128_si64(__m128i source) {
    const std::uint32_t low32 = static_cast<std::uint32_t>(_mm_cvtsi128_si32(source));
    const std::uint32_t high32 = static_cast<std::uint32_t>(_mm_cvtsi128_si32(_mm_srli_epi64(source, 32)));
    return static_cast<std::int64_t>((static_cast<std::uint64_t>(high32) << 32) | low32);
}
#endif

#if (!defined(__clang__) && defined(_MSC_VER) && _MSC_VER <= 1900) || (!defined(__clang__) && defined(__GNUC__) && \
__GNUC__ < 9) || (defined(__clang__) && __clang_major__ < 8)
    inline void _mm_storeu_si64(void* dest, __m128i source) {
        const int64_t value = _mm_cvtsi128_si64(source);
        std::memcpy(dest, &value, sizeof(value));
    }
#endif

#endif  // defined(TRIMD_ENABLE_AVX) || defined(TRIMD_ENABLE_SSE)
// *INDENT-ON*
