// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetalRHIPrivate.h"

#include "MetalDebugCommandEncoder.h"
#include "MetalCommandBuffer.h"
#include "MetalFence.h"

#if MTLPP_CONFIG_VALIDATE && METAL_DEBUG_OPTIONS
extern int32 GMetalRuntimeDebugLevel;

@implementation FMetalDebugCommandEncoder

APPLE_PLATFORM_OBJECT_ALLOC_OVERRIDES(FMetalDebugCommandEncoder)

-(instancetype)init
{
	id Self = [super init];
	if (Self)
	{
		UpdatedFences = [[NSHashTable weakObjectsHashTable] retain];
		WaitingFences = [[NSHashTable weakObjectsHashTable] retain];
	}
	return Self;
}

-(void)dealloc
{
	[UpdatedFences release];
	[WaitingFences release];
	[super dealloc];
}
@end

FMetalCommandEncoderDebugging::FMetalCommandEncoderDebugging()
{
	
}

FMetalCommandEncoderDebugging::FMetalCommandEncoderDebugging(FMetalDebugCommandEncoder* handle)
: ns::Object<FMetalDebugCommandEncoder*>(handle)
{
	
}

void FMetalCommandEncoderDebugging::AddUpdateFence(id Fence)
{
	if ((EMetalDebugLevel)GMetalRuntimeDebugLevel >= EMetalDebugLevelValidation && Fence)
	{
		[m_ptr->UpdatedFences addObject:(FMetalDebugFence*)Fence];
		[(FMetalDebugFence*)Fence updatingEncoder:m_ptr];
	}
}

void FMetalCommandEncoderDebugging::AddWaitFence(id Fence)
{
	if ((EMetalDebugLevel)GMetalRuntimeDebugLevel >= EMetalDebugLevelValidation && Fence)
	{
		[m_ptr->WaitingFences addObject:(FMetalDebugFence*)Fence];
		[(FMetalDebugFence*)Fence waitingEncoder:m_ptr];
	}
}
#endif

