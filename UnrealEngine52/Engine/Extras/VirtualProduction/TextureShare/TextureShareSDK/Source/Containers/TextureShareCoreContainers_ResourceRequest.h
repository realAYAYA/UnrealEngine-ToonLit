// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Containers/TextureShareCoreContainers_ResourceDesc.h"

/**
 * Resource request data
 * When no size and/or format is specified, values from another process or values from local resources are used.
 * The structure is serializable (binary compatible) and reflected on the SDK side (UE types will be replaced with simplified copies from the SDK).
 */
struct FTextureShareCoreResourceRequest
	: public ITextureShareSerialize
{
	// Resource info
	FTextureShareCoreResourceDesc ResourceDesc;

	// MGPU support. When a texture is rendered on a GPU other than the destination,
	// it must be transferred between GPUs.
	// The transfer is performed on the UE side in the TextureShare module.
	int32 GPUIndex = -1;

	// Required texture format
	// The UE process only uses the "EPixelFormat" value.
	// Otherwise, find the best of "EPixelFormat" associated with "DXGI_FORMAT".
	EPixelFormat PixelFormat = EPixelFormat::PF_Unknown;
	DXGI_FORMAT       Format = DXGI_FORMAT_UNKNOWN;

	// Required texture size (or zero if the original value is acceptable)
	FIntPoint           Size = FIntPoint::ZeroValue;

	// Experimental: nummips feature
	uint32 NumMips = 0;

public:
	virtual ~FTextureShareCoreResourceRequest() = default;

	virtual ITextureShareSerializeStream& Serialize(ITextureShareSerializeStream & Stream) override
	{
		return Stream << ResourceDesc << GPUIndex << PixelFormat << Format << Size << NumMips;
	}

public:
	FTextureShareCoreResourceRequest() = default;

	FTextureShareCoreResourceRequest(const FTextureShareCoreResourceDesc& InResourceDesc)
		: ResourceDesc(InResourceDesc)
	{ }

	FTextureShareCoreResourceRequest(const FTextureShareCoreResourceDesc& InResourceDesc, const FIntPoint& InSize, const DXGI_FORMAT InFormat = DXGI_FORMAT_UNKNOWN, const EPixelFormat InPixelFormat = EPixelFormat::PF_Unknown)
		: ResourceDesc(InResourceDesc)
		, PixelFormat(InPixelFormat)
		, Format(InFormat)
		, Size(InSize)
	{ }

	FTextureShareCoreResourceRequest(const FTextureShareCoreResourceDesc& InResourceDesc, const DXGI_FORMAT InFormat)
		: ResourceDesc(InResourceDesc)
		, Format(InFormat)
	{ }

	void SetPixelFormat(uint32 InPixelFormat)
	{
		PixelFormat = (EPixelFormat)InPixelFormat;
	}

	bool EqualsFunc(const FTextureShareCoreResourceDesc& InResourceDesc) const
	{
		return ResourceDesc.EqualsFunc(InResourceDesc);
	}

	bool EqualsFunc(const FTextureShareCoreViewDesc& InViewDesc) const
	{
		return ResourceDesc.EqualsFunc(InViewDesc);
	}

	bool operator==(const FTextureShareCoreResourceRequest& InResourceRequest) const
	{
		return ResourceDesc == InResourceRequest.ResourceDesc;
	}
};
