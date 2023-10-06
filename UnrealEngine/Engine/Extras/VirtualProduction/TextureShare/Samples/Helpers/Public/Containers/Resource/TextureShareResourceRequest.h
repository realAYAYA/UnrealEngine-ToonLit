// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Containers/TextureShareSDKContainers.h"
#include "Containers/Resource/TextureShareCustomResource.h"

/**
 * Resource request
 */
struct FTextureShareResourceRequest
	: public FTextureShareCoreResourceRequest
{
public:
	FTextureShareResourceRequest(const FTextureShareCoreResourceDesc& InResourceDesc, const FTextureShareCustomResource& InCustomResource)
		: FTextureShareCoreResourceRequest(InResourceDesc, InCustomResource.CustomSize, InCustomResource.CustomFormat)
	{ }

	FTextureShareResourceRequest(const FTextureShareCoreResourceDesc& InResourceDesc, const DXGI_FORMAT InCustomFormat)
		: FTextureShareCoreResourceRequest(InResourceDesc, InCustomFormat)
	{ }

	FTextureShareResourceRequest(const FTextureShareCoreResourceDesc& InResourceDesc, const FIntPoint& InSize, const DXGI_FORMAT InFormat = DXGI_FORMAT_UNKNOWN)
		: FTextureShareCoreResourceRequest(InResourceDesc, InSize, InFormat)
	{ }
};
