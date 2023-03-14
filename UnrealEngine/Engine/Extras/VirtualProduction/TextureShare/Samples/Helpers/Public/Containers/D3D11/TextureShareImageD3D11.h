// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "ITextureShareImage.h"
#include <d3d11.h>
#include <d3d11_1.h>

/**
 * D3D11 texture ref container
 */
struct FTextureShareImageD3D11
	: public ITextureShareImage
{
public:
	FTextureShareImageD3D11(ID3D11Texture2D* InTexture)
		: Texture(InTexture)
	{ }

	virtual ~FTextureShareImageD3D11() = default;

	virtual ETextureShareDeviceType GetDeviceType() const override
	{
		return ETextureShareDeviceType::D3D11;
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
	// D3D11 texture
	ID3D11Texture2D* const Texture = nullptr;
};
