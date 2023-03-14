// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UnsyncCommon.h"

namespace unsync {

#ifndef UNSYNC_USE_DEBUG_HEAP
#	define UNSYNC_USE_DEBUG_HEAP 0
#endif

static constexpr size_t UNSYNC_MALLOC_ALIGNMENT = 16;

enum class EMallocType
{
	Invalid,
	Default,
	Debug
};

void  UnsyncMallocInit(EMallocType MallocType);
void* UnsyncMalloc(size_t Size);
void  UnsyncFree(void* Ptr);

}  // namespace unsync
