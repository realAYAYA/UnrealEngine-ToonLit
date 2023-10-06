// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "ITextureShareResource.h"
#include "Containers/Resource/TextureShareCustomResource.h"
#include <d3d12.h>

/**
 * Render SRV resources
 */
enum class EResourceSRV : uint8
{
	SRVHeapFirstIndex = 0,

	// Resource srv views
	texture1,
	texture2,

	COUNT
};

/**
 * D3D12 texture resource container
 */
struct FTextureShareResourceD3D12
	: public  ITextureShareResource
	, public FTextureShareCustomResource
{
public:
	FTextureShareResourceD3D12(const EResourceSRV InSRVIndex, uint32 InNodeMask = 0)
		: bCreateSRV(true), SRVIndex((uint32)InSRVIndex), NodeMask(InNodeMask)
	{ }

	FTextureShareResourceD3D12(const EResourceSRV InSRVIndex, const FIntPoint& InCustomSize, const DXGI_FORMAT InCustomFormat = DXGI_FORMAT::DXGI_FORMAT_UNKNOWN, uint32 InNodeMask = 0)
		: bCreateSRV(true), SRVIndex((uint32)InSRVIndex), FTextureShareCustomResource(InCustomSize, InCustomFormat), NodeMask(InNodeMask)
	{ }

	FTextureShareResourceD3D12(const EResourceSRV InSRVIndex, const DXGI_FORMAT InCustomFormat, uint32 InNodeMask = 0)
		: bCreateSRV(true), SRVIndex((uint32)InSRVIndex), FTextureShareCustomResource(InCustomFormat), NodeMask(InNodeMask)
	{ }

	virtual ~FTextureShareResourceD3D12()
	{
		Release();
	}

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

	void Release()
	{
		if (Texture)
		{
			Texture->Release();
			Texture = nullptr;
		}
	}

public:
	// create SRV for receiver texture
	const bool bCreateSRV = true;
	const uint32 SRVIndex = 0;

	//@todo handle multi-adapter system
	const uint32 NodeMask = 0;

	ID3D12Resource* Texture = nullptr;
};
