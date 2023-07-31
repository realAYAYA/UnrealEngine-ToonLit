// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
TextureMipAllocator.cpp: Base class for implementing a mip allocation strategy used by FTextureStreamIn.
=============================================================================*/

#include "Streaming/TextureMipAllocator.h"
#include "Engine/Texture.h"

FTextureMipAllocator::FTextureMipAllocator(UTexture* Texture, ETickState InTickState, ETickThread InTickThread)
	: ResourceState(Texture->GetStreamableResourceState())
	, CurrentFirstLODIdx(Texture->GetStreamableResourceState().ResidentFirstLODIdx())
	, PendingFirstLODIdx(Texture->GetStreamableResourceState().RequestedFirstLODIdx())
	, NextTickState(InTickState)
	, NextTickThread(InTickThread) 
{
}
