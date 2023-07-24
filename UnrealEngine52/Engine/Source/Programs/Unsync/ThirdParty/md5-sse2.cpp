#ifdef _MSC_VER
#pragma warning(disable: 4244)
#endif

#include <string.h>

#include <mmintrin.h>
#include <emmintrin.h>

#include "md5-sse2.h"

#define S11  7
#define S12 12
#define S13 17
#define S14 22
#define S21  5
#define S22  9
#define S23 14
#define S24 20
#define S31  4
#define S32 11
#define S33 16
#define S34 23
#define S41  6
#define S42 10
#define S43 15
#define S44 21

#define T1  0xD76AA478
#define T2  0xE8C7B756
#define T3  0x242070DB
#define T4  0xC1BDCEEE
#define T5  0xF57C0FAF
#define T6  0x4787C62A
#define T7  0xA8304613
#define T8  0xFD469501
#define T9  0x698098D8
#define T10 0x8B44F7AF
#define T11 0xFFFF5BB1
#define T12 0x895CD7BE
#define T13 0x6B901122
#define T14 0xFD987193
#define T15 0xA679438E
#define T16 0x49B40821
#define T17 0xF61E2562
#define T18 0xC040B340
#define T19 0x265E5A51
#define T20 0xE9B6C7AA
#define T21 0xD62F105D
#define T22 0x02441453
#define T23 0xD8A1E681
#define T24 0xE7D3FBC8
#define T25 0x21E1CDE6
#define T26 0xC33707D6
#define T27 0xF4D50D87
#define T28 0x455A14ED
#define T29 0xA9E3E905
#define T30 0xFCEFA3F8
#define T31 0x676F02D9
#define T32 0x8D2A4C8A
#define T33 0xFFFA3942
#define T34 0x8771F681
#define T35 0x6D9D6122
#define T36 0xFDE5380C
#define T37 0xA4BEEA44
#define T38 0x4BDECFA9
#define T39 0xF6BB4B60
#define T40 0xBEBFBC70
#define T41 0x289B7EC6
#define T42 0xEAA127FA
#define T43 0xD4EF3085
#define T44 0x04881D05
#define T45 0xD9D4D039
#define T46 0xE6DB99E5
#define T47 0x1FA27CF8
#define T48 0xC4AC5665
#define T49 0xF4292244
#define T50 0x432AFF97
#define T51 0xAB9423A7
#define T52 0xFC93A039
#define T53 0x655B59C3
#define T54 0x8F0CCC92
#define T55 0xFFEFF47D
#define T56 0x85845DD1
#define T57 0x6FA87E4F
#define T58 0xFE2CE6E0
#define T59 0xA3014314
#define T60 0x4E0811A1
#define T61 0xF7537E82
#define T62 0xBD3AF235
#define T63 0x2AD7D2BB
#define T64 0xEB86D391

#define ROTL_SSE2(x, n) { \
    __m128i s; \
    s = _mm_srli_epi32(x, 32 - n); \
    x = _mm_slli_epi32(x, n); \
    x = _mm_or_si128(x, s); \
};

#define ROTL(x, n) ((x << n) | (x >> (32 - n)))

#define F_SSE2(x, y, z) _mm_or_si128(_mm_and_si128(x, y), _mm_andnot_si128(x, z))
#define G_SSE2(x, y, z) _mm_or_si128(_mm_and_si128(x, z), _mm_andnot_si128(z, y))
#define H_SSE2(x, y, z) _mm_xor_si128(_mm_xor_si128(x, y), z)
#define I_SSE2(x, y, z) _mm_xor_si128(y, _mm_or_si128(x, _mm_andnot_si128(z, _mm_set1_epi32(0xffffffff))))

#define F(x, y, z) ((x & y) | (~x & z))
#define G(x, y, z) ((x & z) | (y & ~z))
#define H(x, y, z) (x ^ y ^ z)
#define I(x, y, z) (y ^ (x | ~z))

#define SET_SSE2(step, a, b, c, d, x, s, ac) { \
    a = _mm_add_epi32(_mm_add_epi32(a, _mm_add_epi32(x, _mm_set1_epi32(T##ac))), step##_SSE2(b, c, d)); \
    ROTL_SSE2(a, s); \
    a = _mm_add_epi32(a, b); \
}

#define SET(step, a, b, c, d, x, s, ac) { \
    a += step(b, c, d) + x + T##ac;       \
    a = ROTL(a, s) + b;                   \
}

#define A 0x67452301
#define B 0xefcdab89
#define C 0x98badcfe
#define D 0x10325476

#define GET_PMD5_DATA(dest, src, pos) {      \
    uint32_t v0 =                            \
        ((uint32_t) src[0][pos + 0]) <<  0 | \
        ((uint32_t) src[0][pos + 1]) <<  8 | \
        ((uint32_t) src[0][pos + 2]) << 16 | \
        ((uint32_t) src[0][pos + 3]) << 24;  \
                                             \
    uint32_t v1 =                            \
        ((uint32_t) src[1][pos + 0]) <<  0 | \
        ((uint32_t) src[1][pos + 1]) <<  8 | \
        ((uint32_t) src[1][pos + 2]) << 16 | \
        ((uint32_t) src[1][pos + 3]) << 24;  \
                                             \
    uint32_t v2 =                            \
        ((uint32_t) src[2][pos + 0]) <<  0 | \
        ((uint32_t) src[2][pos + 1]) <<  8 | \
        ((uint32_t) src[2][pos + 2]) << 16 | \
        ((uint32_t) src[2][pos + 3]) << 24;  \
                                             \
    uint32_t v3 =                            \
        ((uint32_t) src[3][pos + 0]) <<  0 | \
        ((uint32_t) src[3][pos + 1]) <<  8 | \
        ((uint32_t) src[3][pos + 2]) << 16 | \
        ((uint32_t) src[3][pos + 3]) << 24;  \
                                             \
    dest = _mm_setr_epi32(v0, v1, v2, v3);   \
}

#define GET_MD5_DATA(dest, src, pos)         \
    dest =                                   \
        ((uint32_t) src[pos + 0]) <<  0 |    \
        ((uint32_t) src[pos + 1]) <<  8 |    \
        ((uint32_t) src[pos + 2]) << 16 |    \
        ((uint32_t) src[pos + 3]) << 24

#define PUT_MD5_DATA(dest, val, pos) {       \
    dest[pos + 0] = (val >>  0) & 0xff;      \
    dest[pos + 1] = (val >>  8) & 0xff;      \
    dest[pos + 2] = (val >> 16) & 0xff;      \
    dest[pos + 3] = (val >> 24) & 0xff;      \
}

const static uint8_t md5_padding[64] = {
    0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

pmd5_status pmd5_init_all(pmd5_context * ctx) {
    ctx->len[0] = 0;
    ctx->len[1] = 0;
    ctx->len[2] = 0;
    ctx->len[3] = 0;

    ctx->state[0] = _mm_set1_epi32(A);
    ctx->state[1] = _mm_set1_epi32(B);
    ctx->state[2] = _mm_set1_epi32(C);
    ctx->state[3] = _mm_set1_epi32(D);

    return PMD5_SUCCESS;
}

pmd5_status pmd5_init_slot(pmd5_context * ctx, pmd5_slot slot) {
    __m128i mask = {};
    __m128i values[4];

    if ((slot > 3) || (slot < 0))
        return PMD5_INVALID_SLOT;

    ctx->len[slot] = 0;

    switch (slot) {
    case PMD5_SLOT0:
        values[0] = _mm_cvtsi32_si128(A);
        values[1] = _mm_cvtsi32_si128(B);
        values[2] = _mm_cvtsi32_si128(C);
        values[3] = _mm_cvtsi32_si128(D);
        mask = _mm_cvtsi32_si128(0xffffffff);
        break;
    case PMD5_SLOT1:
        values[0] = _mm_slli_si128(_mm_cvtsi32_si128(A),  4);
        values[1] = _mm_slli_si128(_mm_cvtsi32_si128(B),  4);
        values[2] = _mm_slli_si128(_mm_cvtsi32_si128(C),  4);
        values[3] = _mm_slli_si128(_mm_cvtsi32_si128(D),  4);
        mask = _mm_slli_si128(_mm_cvtsi32_si128(0xffffffff),  4);
        break;
    case PMD5_SLOT2:
        values[0] = _mm_slli_si128(_mm_cvtsi32_si128(A),  8);
        values[1] = _mm_slli_si128(_mm_cvtsi32_si128(B),  8);
        values[2] = _mm_slli_si128(_mm_cvtsi32_si128(C),  8);
        values[3] = _mm_slli_si128(_mm_cvtsi32_si128(D),  8);
        mask = _mm_slli_si128(_mm_cvtsi32_si128(0xffffffff),  8);
        break;
    case PMD5_SLOT3:
        values[0] = _mm_slli_si128(_mm_cvtsi32_si128(A), 12);
        values[1] = _mm_slli_si128(_mm_cvtsi32_si128(B), 12);
        values[2] = _mm_slli_si128(_mm_cvtsi32_si128(C), 12);
        values[3] = _mm_slli_si128(_mm_cvtsi32_si128(D), 12);
        mask = _mm_slli_si128(_mm_cvtsi32_si128(0xffffffff), 12);
        break;
    }

    ctx->state[0] = _mm_or_si128(values[0], _mm_andnot_si128(mask, ctx->state[0]));
    ctx->state[1] = _mm_or_si128(values[1], _mm_andnot_si128(mask, ctx->state[1]));
    ctx->state[2] = _mm_or_si128(values[2], _mm_andnot_si128(mask, ctx->state[2]));
    ctx->state[3] = _mm_or_si128(values[3], _mm_andnot_si128(mask, ctx->state[3]));

    return PMD5_SUCCESS;
}

static inline void pmd5_process(pmd5_context * ctx, const uint8_t * data[4]) {
    __m128i W[16], a, b, c, d;

#ifdef NO_PTR_ALIASING
    GET_PMD5_DATA(W[ 0], data,  0);
    GET_PMD5_DATA(W[ 1], data,  4);
    GET_PMD5_DATA(W[ 2], data,  8);
    GET_PMD5_DATA(W[ 3], data, 12);
    GET_PMD5_DATA(W[ 4], data, 16);
    GET_PMD5_DATA(W[ 5], data, 20);
    GET_PMD5_DATA(W[ 6], data, 24);
    GET_PMD5_DATA(W[ 7], data, 28);
    GET_PMD5_DATA(W[ 8], data, 32);
    GET_PMD5_DATA(W[ 9], data, 36);
    GET_PMD5_DATA(W[10], data, 40);
    GET_PMD5_DATA(W[11], data, 44);
    GET_PMD5_DATA(W[12], data, 48);
    GET_PMD5_DATA(W[13], data, 52);
    GET_PMD5_DATA(W[14], data, 56);
    GET_PMD5_DATA(W[15], data, 60);
#else
    uintptr_t data_ptr;
    __m128i const * vectors[4];
    vectors[0] = (__m128i *) data[0];
    vectors[1] = (__m128i *) data[1];
    vectors[2] = (__m128i *) data[2];
    vectors[3] = (__m128i *) data[3];

    data_ptr =
        (uintptr_t) data[0] |
        (uintptr_t) data[1] |
        (uintptr_t) data[2] |
        (uintptr_t) data[3];

    if (data_ptr & 0x0f) {
        W[ 0] = _mm_loadu_si128(vectors[0] + 0);
        W[ 1] = _mm_loadu_si128(vectors[1] + 0);
        W[ 2] = _mm_loadu_si128(vectors[2] + 0);
        W[ 3] = _mm_loadu_si128(vectors[3] + 0);
        W[ 4] = _mm_loadu_si128(vectors[0] + 1);
        W[ 5] = _mm_loadu_si128(vectors[1] + 1);
        W[ 6] = _mm_loadu_si128(vectors[2] + 1);
        W[ 7] = _mm_loadu_si128(vectors[3] + 1);
        W[ 8] = _mm_loadu_si128(vectors[0] + 2);
        W[ 9] = _mm_loadu_si128(vectors[1] + 2);
        W[10] = _mm_loadu_si128(vectors[2] + 2);
        W[11] = _mm_loadu_si128(vectors[3] + 2);
        W[12] = _mm_loadu_si128(vectors[0] + 3);
        W[13] = _mm_loadu_si128(vectors[1] + 3);
        W[14] = _mm_loadu_si128(vectors[2] + 3);
        W[15] = _mm_loadu_si128(vectors[3] + 3);
    } else {
        W[ 0] = _mm_load_si128(vectors[0] + 0);
        W[ 1] = _mm_load_si128(vectors[1] + 0);
        W[ 2] = _mm_load_si128(vectors[2] + 0);
        W[ 3] = _mm_load_si128(vectors[3] + 0);
        W[ 4] = _mm_load_si128(vectors[0] + 1);
        W[ 5] = _mm_load_si128(vectors[1] + 1);
        W[ 6] = _mm_load_si128(vectors[2] + 1);
        W[ 7] = _mm_load_si128(vectors[3] + 1);
        W[ 8] = _mm_load_si128(vectors[0] + 2);
        W[ 9] = _mm_load_si128(vectors[1] + 2);
        W[10] = _mm_load_si128(vectors[2] + 2);
        W[11] = _mm_load_si128(vectors[3] + 2);
        W[12] = _mm_load_si128(vectors[0] + 3);
        W[13] = _mm_load_si128(vectors[1] + 3);
        W[14] = _mm_load_si128(vectors[2] + 3);
        W[15] = _mm_load_si128(vectors[3] + 3);
    }

    a = _mm_unpacklo_epi32(W[ 0], W[ 1]);
    b = _mm_unpacklo_epi32(W[ 2], W[ 3]);
    c = _mm_unpackhi_epi32(W[ 0], W[ 1]);
    d = _mm_unpackhi_epi32(W[ 2], W[ 3]);

    W[ 0] = _mm_unpacklo_epi64(a, b);
    W[ 1] = _mm_unpackhi_epi64(a, b);
    W[ 2] = _mm_unpacklo_epi64(c, d);
    W[ 3] = _mm_unpackhi_epi64(c, d);

    a = _mm_unpacklo_epi32(W[ 4], W[ 5]);
    b = _mm_unpacklo_epi32(W[ 6], W[ 7]);
    c = _mm_unpackhi_epi32(W[ 4], W[ 5]);
    d = _mm_unpackhi_epi32(W[ 6], W[ 7]);

    W[ 4] = _mm_unpacklo_epi64(a, b);
    W[ 5] = _mm_unpackhi_epi64(a, b);
    W[ 6] = _mm_unpacklo_epi64(c, d);
    W[ 7] = _mm_unpackhi_epi64(c, d);

    a = _mm_unpacklo_epi32(W[ 8], W[ 9]);
    b = _mm_unpacklo_epi32(W[10], W[11]);
    c = _mm_unpackhi_epi32(W[ 8], W[ 9]);
    d = _mm_unpackhi_epi32(W[10], W[11]);

    W[ 8] = _mm_unpacklo_epi64(a, b);
    W[ 9] = _mm_unpackhi_epi64(a, b);
    W[10] = _mm_unpacklo_epi64(c, d);
    W[11] = _mm_unpackhi_epi64(c, d);

    a = _mm_unpacklo_epi32(W[12], W[13]);
    b = _mm_unpacklo_epi32(W[14], W[15]);
    c = _mm_unpackhi_epi32(W[12], W[13]);
    d = _mm_unpackhi_epi32(W[14], W[15]);

    W[12] = _mm_unpacklo_epi64(a, b);
    W[13] = _mm_unpackhi_epi64(a, b);
    W[14] = _mm_unpacklo_epi64(c, d);
    W[15] = _mm_unpackhi_epi64(c, d);
#endif

    a = ctx->state[0];
    b = ctx->state[1];
    c = ctx->state[2];
    d = ctx->state[3];

    SET_SSE2(F, a, b, c, d, W[ 0], S11,  1);
    SET_SSE2(F, d, a, b, c, W[ 1], S12,  2);
    SET_SSE2(F, c, d, a, b, W[ 2], S13,  3);
    SET_SSE2(F, b, c, d, a, W[ 3], S14,  4);
    SET_SSE2(F, a, b, c, d, W[ 4], S11,  5);
    SET_SSE2(F, d, a, b, c, W[ 5], S12,  6);
    SET_SSE2(F, c, d, a, b, W[ 6], S13,  7);
    SET_SSE2(F, b, c, d, a, W[ 7], S14,  8);
    SET_SSE2(F, a, b, c, d, W[ 8], S11,  9);
    SET_SSE2(F, d, a, b, c, W[ 9], S12, 10);
    SET_SSE2(F, c, d, a, b, W[10], S13, 11);
    SET_SSE2(F, b, c, d, a, W[11], S14, 12);
    SET_SSE2(F, a, b, c, d, W[12], S11, 13);
    SET_SSE2(F, d, a, b, c, W[13], S12, 14);
    SET_SSE2(F, c, d, a, b, W[14], S13, 15);
    SET_SSE2(F, b, c, d, a, W[15], S14, 16);

    SET_SSE2(G, a, b, c, d, W[ 1], S21, 17);
    SET_SSE2(G, d, a, b, c, W[ 6], S22, 18);
    SET_SSE2(G, c, d, a, b, W[11], S23, 19);
    SET_SSE2(G, b, c, d, a, W[ 0], S24, 20);
    SET_SSE2(G, a, b, c, d, W[ 5], S21, 21);
    SET_SSE2(G, d, a, b, c, W[10], S22, 22);
    SET_SSE2(G, c, d, a, b, W[15], S23, 23);
    SET_SSE2(G, b, c, d, a, W[ 4], S24, 24);
    SET_SSE2(G, a, b, c, d, W[ 9], S21, 25);
    SET_SSE2(G, d, a, b, c, W[14], S22, 26);
    SET_SSE2(G, c, d, a, b, W[ 3], S23, 27);
    SET_SSE2(G, b, c, d, a, W[ 8], S24, 28);
    SET_SSE2(G, a, b, c, d, W[13], S21, 29);
    SET_SSE2(G, d, a, b, c, W[ 2], S22, 30);
    SET_SSE2(G, c, d, a, b, W[ 7], S23, 31);
    SET_SSE2(G, b, c, d, a, W[12], S24, 32);

    SET_SSE2(H, a, b, c, d, W[ 5], S31, 33);
    SET_SSE2(H, d, a, b, c, W[ 8], S32, 34);
    SET_SSE2(H, c, d, a, b, W[11], S33, 35);
    SET_SSE2(H, b, c, d, a, W[14], S34, 36);
    SET_SSE2(H, a, b, c, d, W[ 1], S31, 37);
    SET_SSE2(H, d, a, b, c, W[ 4], S32, 38);
    SET_SSE2(H, c, d, a, b, W[ 7], S33, 39);
    SET_SSE2(H, b, c, d, a, W[10], S34, 40);
    SET_SSE2(H, a, b, c, d, W[13], S31, 41);
    SET_SSE2(H, d, a, b, c, W[ 0], S32, 42);
    SET_SSE2(H, c, d, a, b, W[ 3], S33, 43);
    SET_SSE2(H, b, c, d, a, W[ 6], S34, 44);
    SET_SSE2(H, a, b, c, d, W[ 9], S31, 45);
    SET_SSE2(H, d, a, b, c, W[12], S32, 46);
    SET_SSE2(H, c, d, a, b, W[15], S33, 47);
    SET_SSE2(H, b, c, d, a, W[ 2], S34, 48);

    SET_SSE2(I, a, b, c, d, W[ 0], S41, 49);
    SET_SSE2(I, d, a, b, c, W[ 7], S42, 50);
    SET_SSE2(I, c, d, a, b, W[14], S43, 51);
    SET_SSE2(I, b, c, d, a, W[ 5], S44, 52);
    SET_SSE2(I, a, b, c, d, W[12], S41, 53);
    SET_SSE2(I, d, a, b, c, W[ 3], S42, 54);
    SET_SSE2(I, c, d, a, b, W[10], S43, 55);
    SET_SSE2(I, b, c, d, a, W[ 1], S44, 56);
    SET_SSE2(I, a, b, c, d, W[ 8], S41, 57);
    SET_SSE2(I, d, a, b, c, W[15], S42, 58);
    SET_SSE2(I, c, d, a, b, W[ 6], S43, 59);
    SET_SSE2(I, b, c, d, a, W[13], S44, 60);
    SET_SSE2(I, a, b, c, d, W[ 4], S41, 61);
    SET_SSE2(I, d, a, b, c, W[11], S42, 62);
    SET_SSE2(I, c, d, a, b, W[ 2], S43, 63);
    SET_SSE2(I, b, c, d, a, W[ 9], S44, 64);

    ctx->state[0] = _mm_add_epi32(ctx->state[0], a);
    ctx->state[1] = _mm_add_epi32(ctx->state[1], b);
    ctx->state[2] = _mm_add_epi32(ctx->state[2], c);
    ctx->state[3] = _mm_add_epi32(ctx->state[3], d);
}

pmd5_status pmd5_update_all_simple(pmd5_context * ctx, const uint8_t * data[4], uint64_t length) {
    const uint8_t * ptrs[4] = { data[0], data[1], data[2], data[3] };

    if (!length) return PMD5_SUCCESS;

    ctx->len[0] += length;
    ctx->len[1] += length;
    ctx->len[2] += length;
    ctx->len[3] += length;

    if (!ptrs[0]) ptrs[0] = md5_padding;
    if (!ptrs[1]) ptrs[1] = md5_padding;
    if (!ptrs[2]) ptrs[2] = md5_padding;
    if (!ptrs[3]) ptrs[3] = md5_padding;

    while (length >= 64) {
        pmd5_process(ctx, ptrs);
        length -= 64;
        if (data[0]) ptrs[0] += 64;
        if (data[1]) ptrs[1] += 64;
        if (data[2]) ptrs[2] += 64;
        if (data[3]) ptrs[3] += 64;
    }

    if (length) return PMD5_UNALIGNED_UPDATE;

    if (data[0]) data[0] = ptrs[0];
    if (data[1]) data[1] = ptrs[1];
    if (data[2]) data[2] = ptrs[2];
    if (data[3]) data[3] = ptrs[3];

    return PMD5_SUCCESS;
}

pmd5_status pmd5_update_all(pmd5_context * ctx, const uint8_t * data[4], uint64_t lengths[4]) {
    uint64_t length = lengths[0];

    if (lengths[1] < length) length = lengths[1];
    if (lengths[2] < length) length = lengths[2];
    if (lengths[3] < length) length = lengths[3];

    lengths[0] -= length;
    lengths[1] -= length;
    lengths[2] -= length;
    lengths[3] -= length;

    return pmd5_update_all_simple(ctx, data, length);
}

pmd5_status pmd5_finish_all(pmd5_context * ctx, uint8_t digests[4][16]) {
    uint8_t padding[4 * 64];
    const uint8_t * ppadding[4];
    __m128i a, b, c, d;
    uint32_t len0 = ctx->len[0] * 8, len1 = ctx->len[1] * 8, len2 = ctx->len[2] * 8, len3 = ctx->len[3] * 8;
    uint32_t v;

    memset(padding, 0, 4 * 64);

    ppadding[0] = &padding[0 * 64];
    ppadding[1] = &padding[1 * 64];
    ppadding[2] = &padding[2 * 64];
    ppadding[3] = &padding[3 * 64];

    padding[0 * 64] = 0x80;
    padding[1 * 64] = 0x80;
    padding[2 * 64] = 0x80;
    padding[3 * 64] = 0x80;

    PUT_MD5_DATA(padding, len0, 0 * 64 + 56);
    PUT_MD5_DATA(padding, len1, 1 * 64 + 56);
    PUT_MD5_DATA(padding, len2, 2 * 64 + 56);
    PUT_MD5_DATA(padding, len3, 3 * 64 + 56);

    pmd5_process(ctx, ppadding);

    a = ctx->state[0];
    b = ctx->state[1];
    c = ctx->state[2];
    d = ctx->state[3];

    v = _mm_cvtsi128_si32(a); PUT_MD5_DATA(digests[0], v,  0);
    v = _mm_cvtsi128_si32(b); PUT_MD5_DATA(digests[0], v,  4);
    v = _mm_cvtsi128_si32(c); PUT_MD5_DATA(digests[0], v,  8);
    v = _mm_cvtsi128_si32(d); PUT_MD5_DATA(digests[0], v, 12);

    a = _mm_srli_si128(a, 4);
    b = _mm_srli_si128(b, 4);
    c = _mm_srli_si128(c, 4);
    d = _mm_srli_si128(d, 4);

    v = _mm_cvtsi128_si32(a); PUT_MD5_DATA(digests[1], v,  0);
    v = _mm_cvtsi128_si32(b); PUT_MD5_DATA(digests[1], v,  4);
    v = _mm_cvtsi128_si32(c); PUT_MD5_DATA(digests[1], v,  8);
    v = _mm_cvtsi128_si32(d); PUT_MD5_DATA(digests[1], v, 12);

    a = _mm_srli_si128(a, 4);
    b = _mm_srli_si128(b, 4);
    c = _mm_srli_si128(c, 4);
    d = _mm_srli_si128(d, 4);

    v = _mm_cvtsi128_si32(a); PUT_MD5_DATA(digests[2], v,  0);
    v = _mm_cvtsi128_si32(b); PUT_MD5_DATA(digests[2], v,  4);
    v = _mm_cvtsi128_si32(c); PUT_MD5_DATA(digests[2], v,  8);
    v = _mm_cvtsi128_si32(d); PUT_MD5_DATA(digests[2], v, 12);

    a = _mm_srli_si128(a, 4);
    b = _mm_srli_si128(b, 4);
    c = _mm_srli_si128(c, 4);
    d = _mm_srli_si128(d, 4);

    v = _mm_cvtsi128_si32(a); PUT_MD5_DATA(digests[3], v,  0);
    v = _mm_cvtsi128_si32(b); PUT_MD5_DATA(digests[3], v,  4);
    v = _mm_cvtsi128_si32(c); PUT_MD5_DATA(digests[3], v,  8);
    v = _mm_cvtsi128_si32(d); PUT_MD5_DATA(digests[3], v, 12);

    return PMD5_SUCCESS;
}

pmd5_status pmd5_finish_slot_with_extra(pmd5_context * pctx, uint8_t digest[16], pmd5_slot slot, const uint8_t * data, uint64_t length) {
    md5_context ctx;

    if ((slot > 3) || (slot < 0))
        return PMD5_INVALID_SLOT;

    pmd5_to_md5(pctx, &ctx, slot);
    md5_update(&ctx, data, length);
    md5_finish(&ctx, digest);

    return PMD5_SUCCESS;
}

pmd5_status pmd5_finish_slot(pmd5_context * pctx, uint8_t digest[16], pmd5_slot slot) {
    return pmd5_finish_slot_with_extra(pctx, digest, slot, NULL, 0);
}

void md5_init(md5_context * ctx) {
    ctx->len = 0;

    ctx->state[0] = A;
    ctx->state[1] = B;
    ctx->state[2] = C;
    ctx->state[3] = D;
}

static inline void md5_process(md5_context * ctx, const uint8_t * data) {
    uint32_t W[16], a, b, c, d;

    GET_MD5_DATA(W[ 0], data,  0);
    GET_MD5_DATA(W[ 1], data,  4);
    GET_MD5_DATA(W[ 2], data,  8);
    GET_MD5_DATA(W[ 3], data, 12);
    GET_MD5_DATA(W[ 4], data, 16);
    GET_MD5_DATA(W[ 5], data, 20);
    GET_MD5_DATA(W[ 6], data, 24);
    GET_MD5_DATA(W[ 7], data, 28);
    GET_MD5_DATA(W[ 8], data, 32);
    GET_MD5_DATA(W[ 9], data, 36);
    GET_MD5_DATA(W[10], data, 40);
    GET_MD5_DATA(W[11], data, 44);
    GET_MD5_DATA(W[12], data, 48);
    GET_MD5_DATA(W[13], data, 52);
    GET_MD5_DATA(W[14], data, 56);
    GET_MD5_DATA(W[15], data, 60);

    a = ctx->state[0];
    b = ctx->state[1];
    c = ctx->state[2];
    d = ctx->state[3];

    SET(F, a, b, c, d, W[ 0], S11,  1);
    SET(F, d, a, b, c, W[ 1], S12,  2);
    SET(F, c, d, a, b, W[ 2], S13,  3);
    SET(F, b, c, d, a, W[ 3], S14,  4);
    SET(F, a, b, c, d, W[ 4], S11,  5);
    SET(F, d, a, b, c, W[ 5], S12,  6);
    SET(F, c, d, a, b, W[ 6], S13,  7);
    SET(F, b, c, d, a, W[ 7], S14,  8);
    SET(F, a, b, c, d, W[ 8], S11,  9);
    SET(F, d, a, b, c, W[ 9], S12, 10);
    SET(F, c, d, a, b, W[10], S13, 11);
    SET(F, b, c, d, a, W[11], S14, 12);
    SET(F, a, b, c, d, W[12], S11, 13);
    SET(F, d, a, b, c, W[13], S12, 14);
    SET(F, c, d, a, b, W[14], S13, 15);
    SET(F, b, c, d, a, W[15], S14, 16);

    SET(G, a, b, c, d, W[ 1], S21, 17);
    SET(G, d, a, b, c, W[ 6], S22, 18);
    SET(G, c, d, a, b, W[11], S23, 19);
    SET(G, b, c, d, a, W[ 0], S24, 20);
    SET(G, a, b, c, d, W[ 5], S21, 21);
    SET(G, d, a, b, c, W[10], S22, 22);
    SET(G, c, d, a, b, W[15], S23, 23);
    SET(G, b, c, d, a, W[ 4], S24, 24);
    SET(G, a, b, c, d, W[ 9], S21, 25);
    SET(G, d, a, b, c, W[14], S22, 26);
    SET(G, c, d, a, b, W[ 3], S23, 27);
    SET(G, b, c, d, a, W[ 8], S24, 28);
    SET(G, a, b, c, d, W[13], S21, 29);
    SET(G, d, a, b, c, W[ 2], S22, 30);
    SET(G, c, d, a, b, W[ 7], S23, 31);
    SET(G, b, c, d, a, W[12], S24, 32);

    SET(H, a, b, c, d, W[ 5], S31, 33);
    SET(H, d, a, b, c, W[ 8], S32, 34);
    SET(H, c, d, a, b, W[11], S33, 35);
    SET(H, b, c, d, a, W[14], S34, 36);
    SET(H, a, b, c, d, W[ 1], S31, 37);
    SET(H, d, a, b, c, W[ 4], S32, 38);
    SET(H, c, d, a, b, W[ 7], S33, 39);
    SET(H, b, c, d, a, W[10], S34, 40);
    SET(H, a, b, c, d, W[13], S31, 41);
    SET(H, d, a, b, c, W[ 0], S32, 42);
    SET(H, c, d, a, b, W[ 3], S33, 43);
    SET(H, b, c, d, a, W[ 6], S34, 44);
    SET(H, a, b, c, d, W[ 9], S31, 45);
    SET(H, d, a, b, c, W[12], S32, 46);
    SET(H, c, d, a, b, W[15], S33, 47);
    SET(H, b, c, d, a, W[ 2], S34, 48);

    SET(I, a, b, c, d, W[ 0], S41, 49);
    SET(I, d, a, b, c, W[ 7], S42, 50);
    SET(I, c, d, a, b, W[14], S43, 51);
    SET(I, b, c, d, a, W[ 5], S44, 52);
    SET(I, a, b, c, d, W[12], S41, 53);
    SET(I, d, a, b, c, W[ 3], S42, 54);
    SET(I, c, d, a, b, W[10], S43, 55);
    SET(I, b, c, d, a, W[ 1], S44, 56);
    SET(I, a, b, c, d, W[ 8], S41, 57);
    SET(I, d, a, b, c, W[15], S42, 58);
    SET(I, c, d, a, b, W[ 6], S43, 59);
    SET(I, b, c, d, a, W[13], S44, 60);
    SET(I, a, b, c, d, W[ 4], S41, 61);
    SET(I, d, a, b, c, W[11], S42, 62);
    SET(I, c, d, a, b, W[ 2], S43, 63);
    SET(I, b, c, d, a, W[ 9], S44, 64);

    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
}

void md5_update(md5_context * ctx, const uint8_t * data, uint64_t length) {
    unsigned fill = ctx->len & 0x3f;

    if (!length)
        return;

    ctx->len += length;

    if (fill) {
        if ((length + fill) >= 64) {
            unsigned stub = 64 - fill;
            memcpy(ctx->buffer + fill, data, stub);
            md5_process(ctx, ctx->buffer);
            data += stub;
            length -= stub;
            fill = 0;
        }
    }

    while (length >= 64) {
        md5_process(ctx, data);
        data += 64;
        length -= 64;
    }

    if (length)
        memcpy(ctx->buffer + fill, data, length);
}

void md5_finish(md5_context * ctx, uint8_t digest[16]) {
    uint8_t size[8];
    uint64_t bit_len = ctx->len * 8;

    size[0] = (bit_len >>  0) & 0xff;
    size[1] = (bit_len >>  8) & 0xff;
    size[2] = (bit_len >> 16) & 0xff;
    size[3] = (bit_len >> 24) & 0xff;
    size[4] = (bit_len >> 32) & 0xff;
    size[5] = (bit_len >> 40) & 0xff;
    size[6] = (bit_len >> 48) & 0xff;
    size[7] = (bit_len >> 56) & 0xff;

    md5_update(ctx, md5_padding, 1 + ((55 - ctx->len) & 0x3f));
    md5_update(ctx, size, 8);

    digest[ 0] = (ctx->state[0] >>  0) & 0xff;
    digest[ 1] = (ctx->state[0] >>  8) & 0xff;
    digest[ 2] = (ctx->state[0] >> 16) & 0xff;
    digest[ 3] = (ctx->state[0] >> 24) & 0xff;
    digest[ 4] = (ctx->state[1] >>  0) & 0xff;
    digest[ 5] = (ctx->state[1] >>  8) & 0xff;
    digest[ 6] = (ctx->state[1] >> 16) & 0xff;
    digest[ 7] = (ctx->state[1] >> 24) & 0xff;
    digest[ 8] = (ctx->state[2] >>  0) & 0xff;
    digest[ 9] = (ctx->state[2] >>  8) & 0xff;
    digest[10] = (ctx->state[2] >> 16) & 0xff;
    digest[11] = (ctx->state[2] >> 24) & 0xff;
    digest[12] = (ctx->state[3] >>  0) & 0xff;
    digest[13] = (ctx->state[3] >>  8) & 0xff;
    digest[14] = (ctx->state[3] >> 16) & 0xff;
    digest[15] = (ctx->state[3] >> 24) & 0xff;
}

pmd5_status md5_to_pmd5(const md5_context * ctx, pmd5_context * pctx, pmd5_slot slot) {
    __m128i mask = {}; // EPIC_MOD: uninitialized local
    __m128i values[4];

    if ((slot > 3) || (slot < 0))
        return PMD5_INVALID_SLOT;

    pctx->len[slot] = ctx->len;

    switch (slot) {
    case PMD5_SLOT0:
        values[0] = _mm_cvtsi32_si128(ctx->state[0]);
        values[1] = _mm_cvtsi32_si128(ctx->state[1]);
        values[2] = _mm_cvtsi32_si128(ctx->state[2]);
        values[3] = _mm_cvtsi32_si128(ctx->state[3]);
        mask = _mm_cvtsi32_si128(0xffffffff);
        break;
    case PMD5_SLOT1:
        values[0] = _mm_slli_si128(_mm_cvtsi32_si128(ctx->state[0]),  4);
        values[1] = _mm_slli_si128(_mm_cvtsi32_si128(ctx->state[1]),  4);
        values[2] = _mm_slli_si128(_mm_cvtsi32_si128(ctx->state[2]),  4);
        values[3] = _mm_slli_si128(_mm_cvtsi32_si128(ctx->state[3]),  4);
        mask = _mm_slli_si128(_mm_cvtsi32_si128(0xffffffff),  4);
        break;
    case PMD5_SLOT2:
        values[0] = _mm_slli_si128(_mm_cvtsi32_si128(ctx->state[0]),  8);
        values[1] = _mm_slli_si128(_mm_cvtsi32_si128(ctx->state[1]),  8);
        values[2] = _mm_slli_si128(_mm_cvtsi32_si128(ctx->state[2]),  8);
        values[3] = _mm_slli_si128(_mm_cvtsi32_si128(ctx->state[3]),  8);
        mask = _mm_slli_si128(_mm_cvtsi32_si128(0xffffffff),  8);
        break;
    case PMD5_SLOT3:
        values[0] = _mm_slli_si128(_mm_cvtsi32_si128(ctx->state[0]), 12);
        values[1] = _mm_slli_si128(_mm_cvtsi32_si128(ctx->state[1]), 12);
        values[2] = _mm_slli_si128(_mm_cvtsi32_si128(ctx->state[2]), 12);
        values[3] = _mm_slli_si128(_mm_cvtsi32_si128(ctx->state[3]), 12);
        mask = _mm_slli_si128(_mm_cvtsi32_si128(0xffffffff), 12);
        break;
    }

    pctx->state[0] = _mm_or_si128(values[0], _mm_andnot_si128(mask, pctx->state[0]));
    pctx->state[1] = _mm_or_si128(values[1], _mm_andnot_si128(mask, pctx->state[1]));
    pctx->state[2] = _mm_or_si128(values[2], _mm_andnot_si128(mask, pctx->state[2]));
    pctx->state[3] = _mm_or_si128(values[3], _mm_andnot_si128(mask, pctx->state[3]));

    return PMD5_SUCCESS;
}

pmd5_status pmd5_to_md5(const pmd5_context * pctx, md5_context * ctx, pmd5_slot slot) {
    __m128i values[4];

    if ((slot > 3) || (slot < 0))
        return PMD5_INVALID_SLOT;

    ctx->len = pctx->len[slot];

    switch (slot) {
    case PMD5_SLOT0:
        values[0] = pctx->state[0];
        values[1] = pctx->state[1];
        values[2] = pctx->state[2];
        values[3] = pctx->state[3];
        break;
    case PMD5_SLOT1:
        values[0] = _mm_srli_si128(pctx->state[0],  4);
        values[1] = _mm_srli_si128(pctx->state[1],  4);
        values[2] = _mm_srli_si128(pctx->state[2],  4);
        values[3] = _mm_srli_si128(pctx->state[3],  4);
        break;
    case PMD5_SLOT2:
        values[0] = _mm_srli_si128(pctx->state[0],  8);
        values[1] = _mm_srli_si128(pctx->state[1],  8);
        values[2] = _mm_srli_si128(pctx->state[2],  8);
        values[3] = _mm_srli_si128(pctx->state[3],  8);
        break;
    case PMD5_SLOT3:
        values[0] = _mm_srli_si128(pctx->state[0], 12);
        values[1] = _mm_srli_si128(pctx->state[1], 12);
        values[2] = _mm_srli_si128(pctx->state[2], 12);
        values[3] = _mm_srli_si128(pctx->state[3], 12);
        break;
    }

    ctx->state[0] = _mm_cvtsi128_si32(values[0]);
    ctx->state[1] = _mm_cvtsi128_si32(values[1]);
    ctx->state[2] = _mm_cvtsi128_si32(values[2]);
    ctx->state[3] = _mm_cvtsi128_si32(values[3]);

    return PMD5_SUCCESS;
}

#ifdef PMD5_TEST
#include <time.h>
#include <sys/time.h>
#include <stdlib.h>
#include <stdio.h>

static const char * const test_pmsgs[4] = {
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789.,",
    "0123456789.,ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz",
    "hijklmnopqrstuvwxyz0123456789.,ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefg",
    "QRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789.,ABCDEFGHIJKLMNOP",
};

static const char * const vectors[7] = {
    "",
    "a",
    "abc",
    "message digest",
    "abcdefghijklmnopqrstuvwxyz",
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789",
    "12345678901234567890123456789012345678901234567890123456789012345678901234567890",
};

static const uint8_t test_results[4][16] = {
    { 0xc9, 0x93, 0x9c, 0xd5, 0x23, 0x2a, 0xb7, 0xe8, 0xf6, 0x62, 0x1f, 0x15, 0x8e, 0x85, 0xd2, 0x39, },
    { 0x9a, 0x1f, 0x2f, 0x88, 0xe3, 0x74, 0x5c, 0x20, 0x12, 0xea, 0x49, 0x85, 0xd5, 0x30, 0x42, 0x01, },
    { 0xd3, 0x9c, 0x0e, 0x08, 0x25, 0xc3, 0xd5, 0x62, 0x7a, 0x55, 0x72, 0x77, 0x2d, 0xc2, 0xa7, 0x2b, },
    { 0x08, 0x41, 0xce, 0x9c, 0xc8, 0x1c, 0x5e, 0x7a, 0x97, 0xea, 0xd8, 0x1a, 0x15, 0xb7, 0x40, 0x90, },
};

static const uint8_t vector_results[7][16] = {
    { 0xd4, 0x1d, 0x8c, 0xd9, 0x8f, 0x00, 0xb2, 0x04, 0xe9, 0x80, 0x09, 0x98, 0xec, 0xf8, 0x42, 0x7e, },
    { 0x0c, 0xc1, 0x75, 0xb9, 0xc0, 0xf1, 0xb6, 0xa8, 0x31, 0xc3, 0x99, 0xe2, 0x69, 0x77, 0x26, 0x61, },
    { 0x90, 0x01, 0x50, 0x98, 0x3c, 0xd2, 0x4f, 0xb0, 0xd6, 0x96, 0x3f, 0x7d, 0x28, 0xe1, 0x7f, 0x72, },
    { 0xf9, 0x6b, 0x69, 0x7d, 0x7c, 0xb7, 0x93, 0x8d, 0x52, 0x5a, 0x2f, 0x31, 0xaa, 0xf1, 0x61, 0xd0, },
    { 0xc3, 0xfc, 0xd3, 0xd7, 0x61, 0x92, 0xe4, 0x00, 0x7d, 0xfb, 0x49, 0x6c, 0xca, 0x67, 0xe1, 0x3b, },
    { 0xd1, 0x74, 0xab, 0x98, 0xd2, 0x77, 0xd9, 0xf5, 0xa5, 0x61, 0x1c, 0x2c, 0x9f, 0x41, 0x9d, 0x9f, },
    { 0x57, 0xed, 0xf4, 0xa2, 0x2b, 0xe3, 0xc9, 0x55, 0xac, 0x49, 0xda, 0x2e, 0x21, 0x07, 0xb6, 0x7a, },
};

int printdigest(const uint8_t digest[16], const uint8_t comp[16]) {
    int i;

    for (i = 0; i < 16; i++) {
        printf("%02x", digest[i]);
    }

    for (i = 0; i < 16; i++) {
        if (digest[i] != comp[i]) {
            printf(" - fail\n");
            return 1;
        }
    }
    printf(" - ok\n");
    return 0;
}

int main(int argc, char ** argv) {
    md5_context ctx;
    pmd5_context pctx;
    const uint8_t * msgs[4] = { test_pmsgs[0], test_pmsgs[1], test_pmsgs[2], test_pmsgs[3] };
    int i;
    int failure = 0;

    uint8_t digest[16];
    uint8_t digests[5][16];

    printf("Well known MD5 test vectors\n");

    for (i = 0; i < 7; i++) {
        const char * msg = vectors[i];
        md5_init(&ctx);
        md5_update(&ctx, msg, strlen(msg));
        md5_finish(&ctx, digest);
        failure |= printdigest(digest, vector_results[i]);
    }

    printf("----\n");

    printf("64-bytes aligned parallel MD5\n");

    pmd5_init_all(&pctx);
    pmd5_update_all_simple(&pctx, msgs, 64);
    pmd5_finish_all(&pctx, digests);

    failure |= printdigest(digests[0], test_results[0]);
    failure |= printdigest(digests[1], test_results[1]);
    failure |= printdigest(digests[2], test_results[2]);
    failure |= printdigest(digests[3], test_results[3]);

    printf("----\n");

    printf("Same with normal MD5\n");

    for (i = 0; i < 4; i++) {
        md5_init(&ctx);
        md5_update(&ctx, test_pmsgs[i], 64);
        md5_finish(&ctx, digest);
        failure |= printdigest(digest, test_results[i]);
    }

    return failure;
}
#endif
