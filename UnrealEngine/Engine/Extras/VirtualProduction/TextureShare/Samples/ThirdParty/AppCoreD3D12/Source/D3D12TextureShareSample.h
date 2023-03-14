// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "ITextureShareObject.h"

class FD3D12TextureShareSample
{
public:
	FD3D12TextureShareSample(ID3D12Resource* InBackBufferTexture);
	~FD3D12TextureShareSample();

public:
	void BeginFrame(const FTextureShareDeviceContextD3D12& InDeviceContext, ID3D12Resource* InBackBufferTexture);
	void EndFrame(const FTextureShareDeviceContextD3D12& InDeviceContext, ID3D12Resource* InBackBufferTexture);

	bool IsFrameSyncActive() const
	{
		return TextureShareObject && TextureShareObject->IsFrameSyncActive();
	}

	int32 GetReceiveTextureSRV(int32 InReceiveTextureIndex) const;

private:
	ITextureShareObject* TextureShareObject = nullptr;
};
