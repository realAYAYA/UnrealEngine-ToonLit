/*
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

#include "pas_config.h"

#if LIBPAS_ENABLED

#include "pas_reservation.h"

#include "pas_enumerable_page_malloc.h"
#include "pas_heap_lock.h"
#include "pas_large_sharing_pool.h"
#include "pas_page_malloc.h"

#ifdef _WIN32
pas_commit_mode pas_reservation_commit_mode = pas_decommitted;
#else
pas_commit_mode pas_reservation_commit_mode = pas_committed;
#endif

pas_aligned_allocation_result pas_reservation_try_allocate_without_deallocating_padding(size_t size, pas_alignment alignment)
{
    return pas_enumerable_page_malloc_try_allocate_without_deallocating_padding(size, alignment, pas_reservation_commit_mode);
}

void pas_reservation_commit(void* base, size_t size)
{
    if (pas_reservation_should_participate_in_sharing())
        pas_page_malloc_commit(base, size, pas_may_mmap);
}

void pas_reservation_convert_to_state(void* base, size_t size, pas_primordial_page_state desired_state)
{
	pas_heap_lock_assert_held();
	switch (desired_state) {
	case pas_primordial_page_is_committed:
		pas_reservation_commit(base, size);
		return;
	case pas_primordial_page_is_decommitted:
		PAS_ASSERT(pas_reservation_commit_mode == pas_decommitted);
		return;
	case pas_primordial_page_is_shared:
		pas_large_sharing_pool_boot_reservation(
			pas_range_create((uintptr_t)base, (uintptr_t)base + size),
			pas_physical_memory_is_locked_by_virtual_range_common_lock,
			pas_may_mmap);
		return;
	}
	PAS_ASSERT(!"Should not be reached");
}

#endif /* LIBPAS_ENABLED */

