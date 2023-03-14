// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalRHIStagingBuffer.cpp: Metal RHI Staging Buffer Class.
=============================================================================*/


#include "MetalRHIPrivate.h"
#include "MetalRHIStagingBuffer.h"


//------------------------------------------------------------------------------

#pragma mark - Metal RHI Staging Buffer Class


FMetalRHIStagingBuffer::FMetalRHIStagingBuffer()
	: FRHIStagingBuffer()
{
	// void
}

FMetalRHIStagingBuffer::~FMetalRHIStagingBuffer()
{
	if (ShadowBuffer)
	{
		SafeReleaseMetalBuffer(ShadowBuffer);
		ShadowBuffer = nil;
	}
}

void *FMetalRHIStagingBuffer::Lock(uint32 Offset, uint32 NumBytes)
{
	check(ShadowBuffer);
	check(!bIsLocked);
	bIsLocked = true;
	uint8* BackingPtr = (uint8*)ShadowBuffer.GetContents();
	return BackingPtr + Offset;
}

void FMetalRHIStagingBuffer::Unlock()
{
	// does nothing in metal.
	check(bIsLocked);
	bIsLocked = false;
}
