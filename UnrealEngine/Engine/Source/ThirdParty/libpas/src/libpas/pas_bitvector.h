/*
 * Copyright (c) 2018-2020 Apple Inc. All rights reserved.
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

#ifndef PAS_BITVECTOR_H
#define PAS_BITVECTOR_H

#include "pas_found_bit_index.h"
#include "pas_utils.h"
#include "ue_include/pas_bitvector_ue.h"

PAS_BEGIN_EXTERN_C;

static inline bool pas_bitvector_get_from_word(unsigned word, size_t index)
{
    return word & PAS_BITVECTOR_BIT_MASK(index);
}

static inline bool pas_bitvector_get(const unsigned* bits, size_t index)
{
    return pas_bitvector_get_from_word(bits[PAS_BITVECTOR_WORD_INDEX(index)], index);
}

static inline bool pas_bitvector_get_from_one_word(unsigned* bits, size_t index)
{
    PAS_TESTING_ASSERT(index < PAS_BITVECTOR_BITS_PER_WORD);
    return pas_bitvector_get_from_word(*bits, index);
}

static inline void pas_bitvector_set_in_word(unsigned* word_ptr, size_t index, bool value)
{
    if (value)
        (*word_ptr) |= PAS_BITVECTOR_BIT_MASK(index);
    else
        (*word_ptr) &= ~PAS_BITVECTOR_BIT_MASK(index);
}

static inline void pas_bitvector_set(unsigned* bits, size_t index, bool value)
{
    pas_bitvector_set_in_word(bits + PAS_BITVECTOR_WORD_INDEX(index), index, value);
}

static inline void pas_bitvector_set_in_one_word(unsigned* bits, size_t index, bool value)
{
    PAS_TESTING_ASSERT(index < PAS_BITVECTOR_BITS_PER_WORD);
    pas_bitvector_set_in_word(bits, index, value);
}

/* Returns whether we changed the value. */
static inline bool pas_bitvector_set_atomic_in_word(unsigned* word_ptr, size_t index, bool value)
{
    for (;;) {
        unsigned old_value;
        unsigned new_value;
        
        old_value = *word_ptr;

        new_value = old_value;
        pas_bitvector_set_in_word(&new_value, index, value);
        
        if (old_value == new_value)
            return false;
        
        if (pas_compare_and_swap_uint32_weak(word_ptr, old_value, new_value))
            return true;
    }
}

static inline bool pas_bitvector_set_atomic(unsigned* bits, size_t index, bool value)
{
    return pas_bitvector_set_atomic_in_word(
        bits + PAS_BITVECTOR_WORD_INDEX(index), index, value);
}

static inline pas_found_bit_index pas_bitvector_find_first_set(const unsigned* bits,
                                                               size_t word_index,
                                                               size_t num_words)
{
    do {
        unsigned word;
        
        word = bits[word_index];
        
        if (word) {
            return pas_found_bit_index_create(
                PAS_BITVECTOR_BIT_INDEX(word_index) + (size_t)pas_count_trailing_zeroes32(word),
                word_index,
                word);
        }
        
        word_index++;
    } while (word_index < num_words);
    
    return pas_found_bit_index_create_empty();
}

typedef unsigned (*pas_bitvector_bits_source)(size_t word_index, void* arg);

typedef bool (*pas_bitvector_for_each_set_bit_callback)(pas_found_bit_index index,
                                                        void* arg);

static PAS_ALWAYS_INLINE bool pas_bitvector_for_each_set_bit(
    pas_bitvector_bits_source bits_source,
    size_t start_word_index,
    size_t end_word_index,
    pas_bitvector_for_each_set_bit_callback callback,
    void* arg)
{
    size_t word_index;

    for (word_index = start_word_index; word_index < end_word_index; ++word_index) {
        unsigned word;
        size_t base_index;

        word = bits_source(word_index, arg);

        base_index = PAS_BITVECTOR_BIT_INDEX(word_index);

        while (word) {
            unsigned index_offset;

            index_offset = (unsigned)pas_count_trailing_zeroes32(word);

            if (!callback(
                    pas_found_bit_index_create(base_index + index_offset, word_index, word),
                    arg))
                return false;
            
            pas_bitvector_set_in_word(&word, index_offset, false);
        }
    }

    return true;
}

/* Remember: in little-endian, a 64-bit bitvector and a 32-bit bitvector are actually the same
   thing *except* that the 32-bit bitvector may not have a whole number of 64-bit words. */
static PAS_ALWAYS_INLINE void pas_bitvector64_set_range(
    uint64_t* bits64,
    size_t begin, /* inclusive */
    size_t end, /* exclusive */
    bool value)
{
    size_t begin_word_index;
    size_t end_word_index;
    size_t begin_bit_shift;
    size_t end_bit_shift;
    size_t index;

    begin_word_index = PAS_BITVECTOR_WORD64_INDEX(begin);
    begin_bit_shift = PAS_BITVECTOR_BIT_SHIFT64(begin);

    end_word_index = PAS_BITVECTOR_WORD64_INDEX(end);
    end_bit_shift = PAS_BITVECTOR_BIT_SHIFT64(end);

    if (begin_bit_shift) {
        uint64_t mask;

        mask = UINT64_MAX << begin_bit_shift;
        
        if (value)
            bits64[begin_word_index] |= mask;
        else
            bits64[begin_word_index] &= ~mask;
        
        begin_word_index++;
    }

    for (index = begin_word_index; index < end_word_index; ++index)
        bits64[index] = value ? UINT64_MAX : 0;

    if (end_bit_shift) {
        uint64_t mask;

        mask = pas_make_mask64(end_bit_shift);
        
        if (value)
            bits64[end_word_index] |= mask;
        else
            bits64[end_word_index] &= ~mask;
    }
}

PAS_END_EXTERN_C;

#endif /* PAS_BITVECTOR_H */

