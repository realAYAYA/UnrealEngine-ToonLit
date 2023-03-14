// Copyright Epic Games, Inc. All Rights Reserved.

#include "Resources/TextureShareResource.h"
#include "Containers/TextureShareContainers.h"

#include "ITextureShareCoreObject.h"

#include "RHI.h"
#include "RenderResource.h"

//////////////////////////////////////////////////////////////////////////////////////////////
// FTextureShareResource
//////////////////////////////////////////////////////////////////////////////////////////////
FTextureShareResource::FTextureShareResource(const TSharedRef<ITextureShareCoreObject, ESPMode::ThreadSafe>& InCoreObject, const FTextureShareCoreResourceDesc& InResourceDesc, const FTextureShareResourceSettings& InResourceSettings)
	: CoreObject(InCoreObject)
	, ResourceDesc(InResourceDesc)
	, ResourceSettings(InResourceSettings)
{
	bSRGB = InResourceSettings.bShouldUseSRGB;

	bGreyScaleFormat = false;
	bIgnoreGammaConversions = true;
}

FTextureShareResource::~FTextureShareResource()
{ }

//////////////////////////////////////////////////////////////////////////////////////////////
bool FTextureShareResource::RegisterResourceHandle(const FTextureShareCoreResourceRequest& InResourceRequest)
{
	switch (CoreObject->GetObjectDesc().ProcessDesc.DeviceType)
	{
	case ETextureShareDeviceType::D3D11:
		return D3D11RegisterResourceHandle(InResourceRequest);

	case ETextureShareDeviceType::D3D12:
		return D3D12RegisterResourceHandle(InResourceRequest);

#if TEXTURESHARE_VULKAN
	case ETextureShareDeviceType::Vulkan:
		return VulkanRegisterResourceHandle(InResourceRequest);
#endif

	default:
		break;
	}

	return false;
}

bool FTextureShareResource::ReleaseTextureShareHandle()
{
	switch (CoreObject->GetObjectDesc().ProcessDesc.DeviceType)
	{
	case ETextureShareDeviceType::D3D11:
		return D3D11ReleaseTextureShareHandle();

	case ETextureShareDeviceType::D3D12:
		return D3D12ReleaseTextureShareHandle();

#if TEXTURESHARE_VULKAN
	case ETextureShareDeviceType::Vulkan:
		return VulkanReleaseTextureShareHandle();
#endif

	default:
		break;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////////////////////////
void FTextureShareResource::InitDynamicRHI()
{
	FTexture2DRHIRef NewTextureRHI;
	InitDynamicRHI_TextureResource2D(NewTextureRHI);

	TextureRHI = (FTextureRHIRef&)NewTextureRHI;
}

void FTextureShareResource::InitDynamicRHI_TextureResource2D(FTexture2DRHIRef& OutTextureRHI)
{
	FRHITextureCreateDesc Desc =
		FRHITextureCreateDesc::Create2D(TEXT("TextureShareResource"), GetSizeX(), GetSizeY(), ResourceSettings.Format)
		.SetClearValue(FClearValueBinding::Black)
		.SetFlags(ETextureCreateFlags::ResolveTargetable | ETextureCreateFlags::Shared);

	// reflect srgb from settings
	if (bSRGB)
	{
		Desc.AddFlags(ETextureCreateFlags::SRGB);
	}

	if (CoreObject->GetObjectDesc().ProcessDesc.DeviceType == ETextureShareDeviceType::Vulkan)
	{
		Desc.AddFlags(ETextureCreateFlags::External);
	}

	OutTextureRHI = RHICreateTexture(Desc);
}
