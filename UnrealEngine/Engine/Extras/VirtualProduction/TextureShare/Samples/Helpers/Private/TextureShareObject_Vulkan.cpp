// Copyright Epic Games, Inc. All Rights Reserved.

#include "TextureShareObject.h"

//////////////////////////////////////////////////////////////////////////////////////////////
// FTextureShareObject
//////////////////////////////////////////////////////////////////////////////////////////////
EResourceState FTextureShareObject::VulkanSendTexture(const FTextureShareDeviceContextVulkan* InDeviceContext, const FTextureShareCoreResourceDesc& InResourceDesc, const FTextureShareImageVulkan* InSrcTexture)
{
	return EResourceState::E_NOT_IMPLEMENTED;
}

EResourceState FTextureShareObject::VulkanReceiveResource(const FTextureShareDeviceContextVulkan* InDeviceContext, const FTextureShareCoreResourceDesc& InResourceDesc, FTextureShareResourceVulkan* InDestResource)
{
	return EResourceState::E_NOT_IMPLEMENTED;
}

EResourceState FTextureShareObject::VulkanReceiveTexture(const FTextureShareDeviceContextVulkan* InDeviceContext, const FTextureShareCoreResourceDesc& InResourceDesc, const FTextureShareImageVulkan* InDestTexture, const FTextureShareTextureCopyParameters& InCopyParameters)
{
	return EResourceState::E_NOT_IMPLEMENTED;
}

FTextureShareCoreResourceRequest FTextureShareObject::GetResourceRequestVulkan(const FTextureShareCoreResourceDesc& InResourceDesc, const FTextureShareImageVulkan* InTexture)
{
	// Not implemented

	return InResourceDesc;
}
