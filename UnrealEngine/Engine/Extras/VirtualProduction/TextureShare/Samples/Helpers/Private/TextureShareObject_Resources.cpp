// Copyright Epic Games, Inc. All Rights Reserved.

#include "TextureShareObject.h"
#include "Misc/TextureShareLog.h"

//////////////////////////////////////////////////////////////////////////////////////////////
// FTextureShareObject
//////////////////////////////////////////////////////////////////////////////////////////////
FTextureShareCoreResourceRequest ITextureShareObject::GetResourceRequest(const FTextureShareCoreResourceDesc& InResourceDesc, const ITextureShareImage& InTexture)
{
	if (InTexture.IsValid())
	{
		switch (InTexture.GetDeviceType())
		{
		case ETextureShareDeviceType::D3D11:  return FTextureShareObject::GetResourceRequestD3D11(InResourceDesc,  static_cast<const FTextureShareImageD3D11*>(InTexture.Ptr()));
		case ETextureShareDeviceType::D3D12:  return FTextureShareObject::GetResourceRequestD3D12(InResourceDesc,  static_cast<const FTextureShareImageD3D12*>(InTexture.Ptr()));
		case ETextureShareDeviceType::Vulkan: return FTextureShareObject::GetResourceRequestVulkan(InResourceDesc, static_cast<const FTextureShareImageVulkan*>(InTexture.Ptr()));
		}
	}

	return InResourceDesc;
}

EResourceState FTextureShareObject::SendTexture(const ITextureShareDeviceContext& InDeviceContext, const FTextureShareCoreResourceDesc& InResourceDesc, const ITextureShareImage& InSrcTexture)
{
	if (!(InDeviceContext.IsValid() && InSrcTexture.IsValid()))
	{
		return EResourceState::E_INVALID_ARGS;
	}

	if (!(InDeviceContext.GetDeviceType() == InSrcTexture.GetDeviceType() && InSrcTexture.GetDeviceType() == ObjectDesc.DeviceType))
	{
		return EResourceState::E_INVALID_DEVICE_TYPE;
	}

	if (TextureShareSDKObject->IsFrameSyncActive() == false)
	{
		return EResourceState::E_FrameSyncLost;
	}

	if (!(ResourceSync_RenderThread(InResourceDesc) && TextureShareSDKObject->IsFrameSyncActive()))
	{
		return EResourceState::E_ResourceSyncError;
	}

	switch (ObjectDesc.DeviceType)
	{
	case ETextureShareDeviceType::D3D11:  return D3D11SendTexture( static_cast<const FTextureShareDeviceContextD3D11*>(InDeviceContext.Ptr()), InResourceDesc,  static_cast<const FTextureShareImageD3D11*>(InSrcTexture.Ptr()));
	case ETextureShareDeviceType::D3D12:  return D3D12SendTexture( static_cast<const FTextureShareDeviceContextD3D12*>(InDeviceContext.Ptr()), InResourceDesc,  static_cast<const FTextureShareImageD3D12*>(InSrcTexture.Ptr()));
	case ETextureShareDeviceType::Vulkan: return VulkanSendTexture(static_cast<const FTextureShareDeviceContextVulkan*>(InDeviceContext.Ptr()), InResourceDesc, static_cast<const FTextureShareImageVulkan*>(InSrcTexture.Ptr()));
	default:
		break;
	}

	return EResourceState::E_UnsupportedDevice;
}

EResourceState FTextureShareObject::ReceiveResource(const ITextureShareDeviceContext& InDeviceContext, const FTextureShareCoreResourceDesc& InResourceDesc, ITextureShareResource& InDestResource)
{
	if (!(InDeviceContext.IsValid()))
	{
		return EResourceState::E_INVALID_ARGS;
	}

	if (!(InDeviceContext.GetDeviceType() == InDestResource.GetDeviceType() && InDestResource.GetDeviceType() == ObjectDesc.DeviceType))
	{
		return EResourceState::E_INVALID_DEVICE_TYPE;
	}

	if (TextureShareSDKObject->IsFrameSyncActive() == false)
	{
		return EResourceState::E_FrameSyncLost;
	}

	if (!(ResourceSync_RenderThread(InResourceDesc) && TextureShareSDKObject->IsFrameSyncActive()))
	{
		return EResourceState::E_ResourceSyncError;
	}

	switch (ObjectDesc.DeviceType)
	{
	case ETextureShareDeviceType::D3D11:  return D3D11ReceiveResource( static_cast<const FTextureShareDeviceContextD3D11*>(InDeviceContext.Ptr()), InResourceDesc,  static_cast<FTextureShareResourceD3D11*>(InDestResource.Ptr()));
	case ETextureShareDeviceType::D3D12:  return D3D12ReceiveResource( static_cast<const FTextureShareDeviceContextD3D12*>(InDeviceContext.Ptr()), InResourceDesc,  static_cast<FTextureShareResourceD3D12*>(InDestResource.Ptr()));
	case ETextureShareDeviceType::Vulkan: return VulkanReceiveResource(static_cast<const FTextureShareDeviceContextVulkan*>(InDeviceContext.Ptr()), InResourceDesc, static_cast<FTextureShareResourceVulkan*>(InDestResource.Ptr()));
	default:
		break;
	}

	return EResourceState::E_UnsupportedDevice;
}


EResourceState FTextureShareObject::ReceiveTexture(const ITextureShareDeviceContext& InDeviceContext, const FTextureShareCoreResourceDesc& InResourceDesc, const ITextureShareImage& InDestTexture, const FTextureShareTextureCopyParameters& InCopyParameters)
{
	if (!(InDeviceContext.IsValid() && InDestTexture.IsValid()))
	{
		return EResourceState::E_INVALID_ARGS;
	}

	if (!(InDeviceContext.GetDeviceType() == InDestTexture.GetDeviceType() && InDestTexture.GetDeviceType() == ObjectDesc.DeviceType))
	{
		return EResourceState::E_INVALID_DEVICE_TYPE;
	}

	if (TextureShareSDKObject->IsFrameSyncActive() == false)
	{
		return EResourceState::E_FrameSyncLost;
	}

	if (!(ResourceSync_RenderThread(InResourceDesc) && TextureShareSDKObject->IsFrameSyncActive()))
	{
		return EResourceState::E_ResourceSyncError;
	}

	switch (ObjectDesc.DeviceType)
	{
	case ETextureShareDeviceType::D3D11:  return D3D11ReceiveTexture( static_cast<const FTextureShareDeviceContextD3D11*>(InDeviceContext.Ptr()), InResourceDesc,  static_cast<const FTextureShareImageD3D11*>(InDestTexture.Ptr()), InCopyParameters);
	case ETextureShareDeviceType::D3D12:  return D3D12ReceiveTexture( static_cast<const FTextureShareDeviceContextD3D12*>(InDeviceContext.Ptr()), InResourceDesc,  static_cast<const FTextureShareImageD3D12*>(InDestTexture.Ptr()), InCopyParameters);
	case ETextureShareDeviceType::Vulkan: return VulkanReceiveTexture(static_cast<const FTextureShareDeviceContextVulkan*>(InDeviceContext.Ptr()), InResourceDesc, static_cast<const FTextureShareImageVulkan*>(InDestTexture.Ptr()), InCopyParameters);
	default:
		break;
	}

	return EResourceState::E_UnsupportedDevice;
}
