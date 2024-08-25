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

#ifndef PAS_RESERVATION_H
#define PAS_RESERVATION_H

#include "pas_aligned_allocation_result.h"
#include "pas_alignment.h"
#include "pas_commit_mode.h"
#include "pas_primordial_page_state.h"

PAS_BEGIN_EXTERN_C;

/* Libpas often wants to ask the OS for a bunch of memory so that it can use some of that memory now and leave the rest for later. This is both
   an important optimization (fewer syscalls when the heap is growing) and an enabler for other optimizations (this is how we do virtual memory
   allocations with large alignment, and then we use that to enable all kinds of efficient bit tricks).
   
   The trouble is that different OSes handle freshly-reserved-but-not-yet-used memory in different ways. Libpas calls this "reservation" memory.
   Such memory has not been touched in any way since being allocated from the OS. This is the one area where libpas's commit/decommit abstraction
   (see pas_page_malloc.h) doesn't adequately handle differences between OSes. This header fills the gap, with the pas_reservation_commit_mode
   and pas_reservation_commit() APIs. These APIs are only meant for managing the commit state of reservation memory.
   
   On some OSes, the commit state of memory doesn't mean anything for memory accounting; all that matters is whether the memory is dirty or
   clean. On those OSes, it makes sense to allocate committed reservations and let the OS trap on page use to make those pages dirty, at which
   point it will really commit them. On other OSes, committed memory is counted against the process whether it's dirty or clean, so it makes
   sense to allocate decomitted reservations, and explicitly commit the memory before use. There are also OSes where the story is more murky.
   
   Hence, we have this pas_reservation_commit_mode, to tell us whether to allocate reservations committed or not. Then we have
   pas_reservation_commit() (see below in this file) for committing the reservation on those OSes that allocate decommitted reservations.

   Here are some more details. We have to deal with three kinds of OS virtual memory APIs:
   
   - Those that have an explicit cheap-to-manipulate commit state and bill all committed memory to the process. Let's call these "Windowish".
   
   - Those that have an explicit cheap-to-manipulate dirty state and bill dirty memory to the process, ignoring committed-and-clean memory.
     Let's call these "Darwinish".
   
   - Those that have an explicit cheap-to-manipulate dirty state, an expensive/complex-to-manipulate commit state, and bill all committed memory
     to the process. Let's call these "Linuxish".
   
   Let's consider the implications for memory in the following states:
   
   - Reservation memory, i.e. memory that was just allocated but not yet used and that may never be used.
   
     This memory would be not-dirty (i.e. clean) on the Darwinish systems. Darwinish systems will not bill the use of this memory against the
     process. Darwinish systems may have some way of reporting how much memory a process has reserved in this way, but they won't use that
     number against the process (will never kill the process for using too much, will never error on allocation because of it). Therefore, on
     Darwinish systems, it makes sense to allocate committed reservations. The unused memory will be clean, and the OS will reflect this in the
     process's memory usage accounting.
     
     On Windowsish systems, we can choose whether reservation memory is committed or not, and it's cheap to do so. If we commit it, then this is
     billed against the process, even if the memory stays clean. If we use too much, we might get errors on allocations (or other processes might
     get errors). Therefore, on Windowish systems, we want to allocate decommitted reservations. Windowsish systems make it cheap to allocate a
     chunk of memory, and have some of it be committed and some of it merely reserved.
     
     On Linuxish systems, the default outcome of a memory allocation is that it is committed and clean. This helps the OS because the OS doesn't
     have to give that memory a physical page, but it's still billed against the process. This seems to imply that on Linuxish systems, we want
     decommitted reservations, so that they aren't billed against the process. Unfortunately, this isn't possible to do scalably on Linux. Asking
     that the memory is decommitted using madvise(FREE or DONTNEED) will do nothing in this case (since that just instructs the OS that the page
     is clean, but the memory stays committed in the sense that it's billed against the process). Linux does have other calls (like munmap) that
     will fully return the pages to the OS (even for accounting purposes), but using those calls in the fine-grained manner that libpas is likely
     to do increases the number of VMAs (virtual memory areas) and there's a hard limit (sometimes as little as 65536) on how many of those you
     can have. Therefore, on Linuxish systems, we'd really love to be able to do something like munmap, but it's a terrible idea, because
     although it would reduce memory usage from an accounting standpoint, it could cause us to artificially run out of memory if we blow out the
     kernel's VMA limit. So, we on Linuxish systems, we don't have a good story for the accounting problem other than libpas will never try to
     create a reservation bigger than 10s of MBs (except if you use cages - but then you're explicitly asking libpas to do the thing that's bad
     for Linuxish systems). Libpas handles Linuxish systems as if they were Darwinish: we let reservations be committed and clean, and accept
     that the reserved memory is counted against us just because we have no good alternative.
     
     FIXME: Maybe find a way to do something smart about cages on Linux. Perhaps just mprotecting them above their high watermark achieves sort
     of what we want on Linux. But the only cage we are using right now is the emergent type one, so maybe who cares.
   
   - Committed memory, i.e. memory that we know we are using.
   
     On Darwinish systems, reservation memory is committed memory, and memory becomes committed if you just touch it. Ditto on Linuxish systems.
     
     On Windowish systems, we need to explicitly commit reservation memory before use. That's what pas_reservation_commit() is for.
     
   - Decommitted memory, i.e. memory that we know we aren't using anymore.
     
     On Darwinish systems, we use an madvise() call that tells the kernel that the memory is now clean. This gets immediately reflected in the
     memory usage accounting used by the OS, because the OS uses the dirty state for accounting.
     
     On Windowsish systems, we use VirtualFree() to decommit the memory. This gets immediately reflected in the memory usage accounting used by
     the OS, bercause the OS uses the commit state for accounting.
     
     On Linuxish systems, we do like Darwinish, even though it's not accounted by the OS for the purposes of deciding whether future memory
     allocations succeed.
   
   It's possible to change this at runtime just before any call into libpas. It's set at compile-time to whatever is appropriate for the platform.
   An incorrect setting will just make performance worse, but ought to have no other effect. The only good reason to ever set it outside libpas
   to anything but libpas's default is for testing. Especially with PAS_ENABLE_TESTING enabled, the commit/decommit calls become mprotect calls
   on Linuxish/Darwinish systems, simulating the stronger Windowsish behavior on Darwinish/Linuxish systems. Therefore, by enabling
   PAS_ENABLE_TESTING and flipping this state, you can test libpas's behavior on any platform regardless of what your platform is. */
PAS_API extern pas_commit_mode pas_reservation_commit_mode;

PAS_API pas_aligned_allocation_result pas_reservation_try_allocate_without_deallocating_padding(size_t size, pas_alignment alignment);

static inline bool pas_reservation_should_participate_in_sharing(void)
{
    return pas_reservation_commit_mode == pas_decommitted;
}

/* This call is for those OSes where we allocate decommitted reservations and commit them before use.
   
   If pas_reservation_commit_mode = pas_committed, then this does nothing. This means that this call does nothing on Darwin and Linux.
   
   If pas_reservation_commit_mode = pas_decommitted, then this commits using pas_page_malloc_commit. */
PAS_API void pas_reservation_commit(void* base, size_t size);

/* This only allows pas_decommitted as a desired state if pas_reservation_commit_mode is pas_decommitted, simply because we have no
   need for decommitting with this function, and it would be weird to do it. */
PAS_API void pas_reservation_convert_to_state(void* base, size_t size, pas_primordial_page_state desired_state);

PAS_END_EXTERN_C;

#endif /* PAS_RESERVATION_H */

