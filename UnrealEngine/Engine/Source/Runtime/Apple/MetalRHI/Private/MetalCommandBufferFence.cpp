// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalCommandBufferFence.cpp: Metal RHI Command Buffer Fence Implementation.
=============================================================================*/

#include "MetalRHIPrivate.h"
#include "MetalCommandBufferFence.h"


//------------------------------------------------------------------------------

#pragma mark - Metal RHI Command Buffer Fence Routines - 


bool FMetalCommandBufferFence::Wait(uint64 Millis)
{
	@autoreleasepool {
		if (CommandBufferFence)
		{
			bool bFinished = CommandBufferFence.Wait(Millis);
			FPlatformMisc::MemoryBarrier();
			return bFinished;
		}
		else
		{
			return true;
		}
	}
}
