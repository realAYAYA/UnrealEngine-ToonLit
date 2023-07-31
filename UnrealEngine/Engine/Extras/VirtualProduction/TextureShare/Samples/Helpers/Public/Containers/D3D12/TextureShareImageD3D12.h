// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "ITextureShareImage.h"
#include <d3d12.h>

/**
 * D3D12 texture ref container
 */
struct FTextureShareImageD3D12
	: public ITextureShareImage
{
public:
	FTextureShareImageD3D12(ID3D12Resource* InTexture, const D3D12_RESOURCE_STATES InTextureState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
		: Texture(InTexture), TextureState(InTextureState)
	{ }

	virtual ~FTextureShareImageD3D12() = default;

	virtual ETextureShareDeviceType GetDeviceType() const override
	{
		return ETextureShareDeviceType::D3D12;
	}

	virtual bool IsValid() const override
	{
		return Texture != nullptr;
	}

	virtual const void* Ptr() const override
	{
		return this;
	}

	virtual void* Ptr() override
	{
		return this;
	}

public:
	// The current state of the texture barrier. Recovering after send\receives
	D3D12_RESOURCE_STATES const TextureState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

	// D3D12 texture
	ID3D12Resource* const Texture = nullptr;
};
