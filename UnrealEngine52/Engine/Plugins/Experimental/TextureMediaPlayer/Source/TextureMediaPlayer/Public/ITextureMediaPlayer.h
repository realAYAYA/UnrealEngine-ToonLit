// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RHI.h"

#if PLATFORM_WINDOWS

THIRD_PARTY_INCLUDES_START
#include "d3d12.h"
THIRD_PARTY_INCLUDES_END

#endif // PLATFORM_WINDOWS

class ITextureMediaPlayer : public TSharedFromThis<ITextureMediaPlayer, ESPMode::ThreadSafe>
{
public:
	virtual ~ITextureMediaPlayer() {}

	virtual void OnFrame(const TArray<uint8>& TextureBuffer, FIntPoint Size) = 0;

#if PLATFORM_WINDOWS
	virtual void OnFrame(FTextureRHIRef TextureRHIRef, TRefCountPtr<ID3D12Fence> D3DFence, uint64 FenceValue) = 0;
#else
	virtual void OnFrame(FTextureRHIRef TextureRHIRef, FGPUFenceRHIRef Fence, uint64 FenceValue) = 0;
#endif // PLATFORM_WINDOWS

};
