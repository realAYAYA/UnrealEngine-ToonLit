// Copyright Epic Games, Inc. All Rights Reserved.

#include "TypedElementDatabaseScratchBuffer.h"

FTypedElementDatabaseScratchBuffer::FTypedElementDatabaseScratchBuffer()
	: CurrentAllocator(&Allocators[0])
	, PreviousAllocator(&Allocators[1])
	, LeastRecentAllocator(&Allocators[2])
{
}

void* FTypedElementDatabaseScratchBuffer::Allocate(size_t Size, size_t Alignment)
{
	return CurrentAllocator.load()->Malloc(Size, Alignment);
}

void FTypedElementDatabaseScratchBuffer::BatchDelete()
{
	MemoryAllocator* Current = CurrentAllocator.exchange(LeastRecentAllocator);
	LeastRecentAllocator = PreviousAllocator;
	PreviousAllocator = Current;
	LeastRecentAllocator->BulkDelete();
}
