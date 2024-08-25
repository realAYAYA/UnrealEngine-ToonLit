// Copyright Epic Games, Inc. All Rights Reserved.

#include "Resources/TextureShareResource.h"
#include "Containers/TextureShareContainers.h"

#include "Module/TextureShareLog.h"
#include "Core/TextureShareCoreHelpers.h"

#include "ITextureShareCoreObject.h"

#include "RHI.h"
#include "RHICommandList.h"
#include "RenderResource.h"

using namespace UE::TextureShareCore;

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
}

FTextureShareResource::~FTextureShareResource()
{ }

const FString& FTextureShareResource::GetCoreObjectName() const
{
	return CoreObject->GetName();
}

//////////////////////////////////////////////////////////////////////////////////////////////
struct FCachedSharedResource
{
	FCachedSharedResource() = default;
	FCachedSharedResource(const void* InNativeResourcePtr, const uint32  InGPUIndex, const FTexture2DRHIRef& InTextureRHI)
		: NativeResourcePtr(InNativeResourcePtr), TextureRHI(InTextureRHI), GPUIndex(InGPUIndex)
	{ }

	const void* NativeResourcePtr = nullptr;
	FTexture2DRHIRef TextureRHI;
	uint32 GPUIndex = 0;

	bool bUnused = false;
};

bool FTextureShareResource::FindCachedSharedResource_RenderThread(void* InNativeResourcePtr, const uint32 InGPUIndex, FTexture2DRHIRef& OutRHIResource) const
{
	if(const FCachedSharedResource* CachedResourcePtr = CachedSharedResources.FindByPredicate([InNativeResourcePtr, InGPUIndex](const FCachedSharedResource& CachedResourceIt)
		{
			return CachedResourceIt.NativeResourcePtr == InNativeResourcePtr && CachedResourceIt.GPUIndex == InGPUIndex;
		}
	))
	{
		OutRHIResource = CachedResourcePtr->TextureRHI;

		return true;
	}

	return false;
}

void FTextureShareResource::AddCachedSharedResource_RenderThread(void* InNativeResourcePtr, const uint32 InGPUIndex, const FTexture2DRHIRef& InRHIResource)
{
	if (InNativeResourcePtr != nullptr && InRHIResource.IsValid())
	{
		CachedSharedResources.Add(FCachedSharedResource(InNativeResourcePtr, InGPUIndex, InRHIResource));
	}
}

void FTextureShareResource::CopyToDestResources_RenderThread(FRHICommandListImmediate& RHICmdList, const TArray<FTextureShareExternalTextureRHI>& InDestResources)
{
	if (InDestResources.Num())
	{
		// Copy params
		FRHICopyTextureInfo Params = {};
		Params.Size = TextureRHI->GetDesc().GetSize();
		Params.SourcePosition = FIntVector(0, 0, 0);
		Params.DestPosition = FIntVector(0, 0, 0);

		for (const FTextureShareExternalTextureRHI& DestTextureIt : InDestResources)
		{
			UE_TS_LOG(LogTextureShareResource, Log, TEXT("%s:CopyToDestResources() from %s.%s to %s.%s (GPU=%d)"), *CoreObject->GetName(), *ResourceDesc.ViewDesc.SrcId, *ResourceDesc.ResourceName, *DestTextureIt.ResourceHandle->ResourceDesc.ViewDesc.Id, *DestTextureIt.ResourceHandle->ResourceDesc.ResourceName, DestTextureIt.GPUIndex);

#if WITH_MGPU
			// UE2UE copy always over GPU0
			SCOPED_GPU_MASK(RHICmdList, FRHIGPUMask::GPU0());
#endif
			RHICmdList.CopyTexture(TextureRHI, DestTextureIt.TextureRHI, Params);
		}
	}
}

void FTextureShareResource::HandleFrameBegin_RenderThread()
{
	for (FCachedSharedResource& ResourceIt : CachedSharedResources)
	{
		ResourceIt.bUnused = true;
	}
}

void FTextureShareResource::HandleFrameEnd_RenderThread()
{
	for (int32 Index = 0; Index < CachedSharedResources.Num(); ++Index)
	{
		if (CachedSharedResources[Index].bUnused)
		{
			CachedSharedResources.RemoveAtSwap(Index, 1, EAllowShrinking::No);
			// RemoveAtSwap func replace the elements in the hole created by the removal with elements from the end of the array, so
			Index--;
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////
bool FTextureShareResource::RegisterResourceHandle_RenderThread(FRHICommandListImmediate& RHICmdList, const FTextureShareCoreResourceRequest& InResourceRequest)
{
	switch (CoreObject->GetObjectDesc_RenderThread().ProcessDesc.DeviceType)
	{
	case ETextureShareDeviceType::D3D11:
		return D3D11RegisterResourceHandle_RenderThread(RHICmdList, InResourceRequest);

	case ETextureShareDeviceType::D3D12:
		return D3D12RegisterResourceHandle_RenderThread(RHICmdList, InResourceRequest);

#if TEXTURESHARE_VULKAN
	case ETextureShareDeviceType::Vulkan:
		return VulkanRegisterResourceHandle_RenderThread(RHICmdList, InResourceRequest);
#endif

	default:
		break;
	}

	return false;
}

bool FTextureShareResource::ReleaseTextureShareHandle_RenderThread()
{
	switch (CoreObject->GetObjectDesc_RenderThread().ProcessDesc.DeviceType)
	{
	case ETextureShareDeviceType::D3D11:
		return D3D11ReleaseTextureShareHandle_RenderThread();

	case ETextureShareDeviceType::D3D12:
		return D3D12ReleaseTextureShareHandle_RenderThread();

#if TEXTURESHARE_VULKAN
	case ETextureShareDeviceType::Vulkan:
		return VulkanReleaseTextureShareHandle_RenderThread();
#endif

	default:
		break;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////////////////////////
void FTextureShareResource::InitRHI(FRHICommandListBase&)
{
	FTexture2DRHIRef NewTextureRHI;
	switch (CoreObject->GetObjectDesc_RenderThread().ProcessDesc.DeviceType)
	{
	case ETextureShareDeviceType::D3D12:
		InitDynamicRHI_D3D12(NewTextureRHI);
		break;

	default:
		InitDynamicRHI_Default(NewTextureRHI);
		break;
	};

	TextureRHI = (FTextureRHIRef&)NewTextureRHI;
}

void FTextureShareResource::InitDynamicRHI_Default(FTexture2DRHIRef& OutTextureRHI)
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

	if (CoreObject->GetObjectDesc_RenderThread().ProcessDesc.DeviceType == ETextureShareDeviceType::Vulkan)
	{
		Desc.AddFlags(ETextureCreateFlags::External);
	}

	OutTextureRHI = RHICreateTexture(Desc);
}
