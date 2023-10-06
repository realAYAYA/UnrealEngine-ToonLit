// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "ITextureShareResource.h"
#include "Containers/Resource/TextureShareCustomResource.h"
#include <d3d11.h>
#include <d3d11_1.h>

/**
 * D3D11 texture resource container
 */
struct FTextureShareResourceD3D11
	: public  ITextureShareResource
	, public FTextureShareCustomResource
{
public:
	FTextureShareResourceD3D11()
		: FTextureShareCustomResource()
	{ }

	FTextureShareResourceD3D11(const FIntPoint & InCustomSize, const bool bCreateSRV = true)
		: bCreateSRV(bCreateSRV), FTextureShareCustomResource(InCustomSize)
	{ }

	FTextureShareResourceD3D11(const FIntPoint& InCustomSize, const DXGI_FORMAT InCustomFormat, const bool bCreateSRV = true)
		: bCreateSRV(bCreateSRV), FTextureShareCustomResource(InCustomSize, InCustomFormat)
	{ }

	FTextureShareResourceD3D11(const DXGI_FORMAT InCustomFormat, const bool bCreateSRV = true)
		: bCreateSRV(bCreateSRV), FTextureShareCustomResource(InCustomFormat)
	{ }

	virtual ~FTextureShareResourceD3D11()
	{
		Release();
	}

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

	ID3D11ShaderResourceView* GetTextureSRV() const
	{
		return TextureSRV;
	}

	void Release()
	{
		if (TextureSRV)
		{
			TextureSRV->Release();
			TextureSRV = nullptr;
		}

		if (Texture)
		{
			Texture->Release();
			Texture = nullptr;
		}
	}

public:
	const bool bCreateSRV = true;

	ID3D11Texture2D* Texture = nullptr;
	ID3D11ShaderResourceView* TextureSRV = nullptr;
	//...
};
