/*
 * Copyright (c) 2019-2021 Apple Inc. All rights reserved.
 * Copyright Epic Games, Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef PAS_SEGREGATED_DIRECTORY_BIT_REFERENCE_H
#define PAS_SEGREGATED_DIRECTORY_BIT_REFERENCE_H

#include "pas_segregated_directory.h"
#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

struct pas_segregated_directory_bit_reference;
typedef struct pas_segregated_directory_bit_reference pas_segregated_directory_bit_reference;

struct pas_segregated_directory_bit_reference {
    pas_segregated_directory_bitvector_segment* segment_ptr;
    size_t index;
    unsigned mask;
    bool is_inline_bit;
};

static inline pas_segregated_directory_bit_reference
pas_segregated_directory_bit_reference_create_null(void)
{
    pas_segregated_directory_bit_reference result;
    result.segment_ptr = NULL;
    result.mask = 0;
    result.is_inline_bit = false;
    result.index = 0;
    return result;
}

static inline pas_segregated_directory_bit_reference
pas_segregated_directory_bit_reference_create_inline(void)
{
    pas_segregated_directory_bit_reference result;
    result.segment_ptr = NULL;
    result.mask = 1;
    result.is_inline_bit = true;
    result.index = 0;
    return result;
}

static inline pas_segregated_directory_bit_reference
pas_segregated_directory_bit_reference_create_out_of_line(
    size_t index,
    pas_segregated_directory_bitvector_segment* segment_ptr,
    unsigned mask)
{
    pas_segregated_directory_bit_reference result;
    result.segment_ptr = segment_ptr;
    result.mask = mask;
    result.is_inline_bit = false;
    result.index = index;
    return result;
}

static inline bool pas_segregated_directory_bit_reference_is_null(
    pas_segregated_directory_bit_reference reference)
{
    return !reference.segment_ptr && !reference.is_inline_bit;
}

static inline bool pas_segregated_directory_bit_reference_is_inline(
    pas_segregated_directory_bit_reference reference)
{
    return reference.is_inline_bit;
}

static inline bool pas_segregated_directory_bit_reference_is_out_of_line(
    pas_segregated_directory_bit_reference reference)
{
    return !!reference.segment_ptr;
}

#define PAS_SEGREGATED_DIRECTORY_BIT_REFERENCE_DEFINE_ACCESSORS(bit_name) \
    static inline bool pas_segregated_directory_bit_reference_get_ ## bit_name( \
        pas_segregated_directory* directory, \
        pas_segregated_directory_bit_reference reference) \
    { \
        PAS_TESTING_ASSERT(!pas_segregated_directory_bit_reference_is_null(reference)); \
        \
        if (pas_segregated_directory_bit_reference_is_inline(reference)) \
            return !!(directory->bits & PAS_SEGREGATED_DIRECTORY_BITS_##bit_name##_MASK); \
        return !!(reference.segment_ptr->bit_name##_bits & reference.mask); \
    } \
    static inline bool pas_segregated_directory_bit_reference_set_ ## bit_name( \
        pas_segregated_directory* directory, \
        pas_segregated_directory_bit_reference reference, \
        bool value) \
    { \
        PAS_TESTING_ASSERT(!pas_segregated_directory_bit_reference_is_null(reference)); \
        \
        if (pas_segregated_directory_bit_reference_is_inline(reference)) { \
            for (;;) { \
                unsigned old_bits; \
                unsigned new_bits; \
                \
                old_bits = directory->bits; \
                if (value) \
                    new_bits = old_bits | PAS_SEGREGATED_DIRECTORY_BITS_##bit_name##_MASK; \
                else \
                    new_bits = old_bits & ~PAS_SEGREGATED_DIRECTORY_BITS_##bit_name##_MASK; \
                \
                if (old_bits == new_bits) \
                    return false; \
                \
                if (pas_compare_and_swap_uint32_weak(&directory->bits, old_bits, new_bits)) \
                    return true; \
            } \
            PAS_ASSERT(!"Should not be reached"); \
        } \
        for (;;) { \
            unsigned old_bits; \
            unsigned new_bits; \
            \
            old_bits = reference.segment_ptr->bit_name##_bits; \
            if (value) \
                new_bits = old_bits | reference.mask; \
            else \
                new_bits = old_bits & ~reference.mask; \
            \
            if (old_bits == new_bits) \
                return false; \
            \
            if (pas_compare_and_swap_uint32_weak( \
                    &reference.segment_ptr->bit_name##_bits, \
                    old_bits, new_bits)) \
                return true; \
        } \
        PAS_ASSERT("!Should not be reached"); \
    } \
    struct pas_dummy

PAS_SEGREGATED_DIRECTORY_BIT_REFERENCE_DEFINE_ACCESSORS(eligible);
PAS_SEGREGATED_DIRECTORY_BIT_REFERENCE_DEFINE_ACCESSORS(empty);

PAS_END_EXTERN_C;

#endif /* PAS_SEGREGATED_DIRECTORY_BIT_REFERENCE_H */

