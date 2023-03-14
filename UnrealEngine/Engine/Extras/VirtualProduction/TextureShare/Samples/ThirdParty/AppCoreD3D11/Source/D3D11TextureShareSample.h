// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "ITextureShareObject.h"

class FD3D11TextureShareSample
{
public:
	FD3D11TextureShareSample(ID3D11Texture2D* InBackBufferTexture);
	~FD3D11TextureShareSample();

public:
	void BeginFrame(const FTextureShareDeviceContextD3D11& InDeviceContext, ID3D11Texture2D* InBackBufferTexture);
	void EndFrame(const FTextureShareDeviceContextD3D11& InDeviceContext, ID3D11Texture2D* InBackBufferTexture);

	bool IsFrameSyncActive() const
	{
		return TextureShareObject && TextureShareObject->IsFrameSyncActive();
	}

	ID3D11ShaderResourceView* GetReceiveTextureSRV(int32 InReceiveTextureIndex) const;

private:
	ITextureShareObject* TextureShareObject = nullptr;
};
