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

#ifndef PAS_PRIMORDIAL_PAGE_STATE_H
#define PAS_PRIMORDIAL_PAGE_STATE_H

#include "pas_commit_mode.h"

PAS_BEGIN_EXTERN_C;

enum pas_primordial_page_state {
	/* The page is reserved by libpas but decommitted. */
	pas_primordial_page_is_decommitted,

	/* The page is tracked by the large sharing pool as free memory, so it may be committed or decommitted. */
	pas_primordial_page_is_shared,

	/* The page is committed. */
	pas_primordial_page_is_committed
};

typedef enum pas_primordial_page_state pas_primordial_page_state;

static inline const char* pas_primordial_page_state_get_string(pas_primordial_page_state state)
{
	switch (state) {
	case pas_primordial_page_is_decommitted:
		return "decommitted";
	case pas_primordial_page_is_shared:
		return "shared";
	case pas_primordial_page_is_committed:
		return "committed";
	}
	PAS_ASSERT(!"Should not be reached");
	return NULL;
}

static inline bool pas_primordial_page_state_knows_commit_mode(pas_primordial_page_state state)
{
	switch (state) {
	case pas_primordial_page_is_decommitted:
	case pas_primordial_page_is_committed:
		return true;
	case pas_primordial_page_is_shared:
		return false;
	}
	PAS_ASSERT(!"Should not be reached");
	return false;
}

static inline pas_commit_mode pas_primordial_page_state_get_commit_mode(pas_primordial_page_state state)
{
	switch (state) {
	case pas_primordial_page_is_decommitted:
		return pas_decommitted;
	case pas_primordial_page_is_committed:
		return pas_committed;
	default:
		PAS_ASSERT(!"Should not be reached");
		return pas_decommitted;
	}
}

static inline pas_primordial_page_state pas_primordial_page_state_for_commit_mode(pas_commit_mode mode)
{
	switch (mode) {
	case pas_decommitted:
		return pas_primordial_page_is_decommitted;
	case pas_committed:
		return pas_primordial_page_is_committed;
	}
	PAS_ASSERT(!"Should not be reached");
	return pas_primordial_page_is_decommitted;
}

PAS_END_EXTERN_C;

#endif /* PAS_PRIMORDIAL_PAGE_STATE_H */

