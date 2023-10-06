// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "Resources/TextureShareResourceSettings.h"
#include "Containers/TextureShareCoreContainers_ResourceDesc.h"
#include "Containers/TextureShareCoreContainers_ResourceHandle.h"

#include "RHI.h"
#include "RHIResources.h"
#include "RenderResource.h"

class ITextureShareCoreObject;
struct FTextureShareCoreResourceRequest;

/**
 * TextureShare external RHI resource
 */
struct FTextureShareExternalTextureRHI
{
	FTextureShareExternalTextureRHI(const FTexture2DRHIRef InTextureRHI, const FTextureShareCoreResourceHandle* InResourceHandle, const uint32 InGPUIndex = 0)
		: TextureRHI(InTextureRHI), ResourceHandle(InResourceHandle), GPUIndex(InGPUIndex)
	{ }

	const FTexture2DRHIRef TextureRHI;
	const FTextureShareCoreResourceHandle* ResourceHandle;
	const uint32 GPUIndex;
};

/**
 * TextureShare RHI resource
 */
class FTextureShareResource
	: public FTexture
{
public:
	FTextureShareResource(const TSharedRef<ITextureShareCoreObject, ESPMode::ThreadSafe>& InCoreObject, const FTextureShareCoreResourceDesc& InResourceDesc, const FTextureShareResourceSettings& InResourceSettings);
	virtual ~FTextureShareResource();

public:
	const FTextureShareCoreResourceDesc& GetResourceDesc() const
	{
		return ResourceDesc;
	}

	const FTextureShareResourceSettings& GetResourceSettings() const
	{
		return ResourceSettings;
	}

	const FTexture2DRHIRef& GetResourceTextureRHI() const
	{
		return (const FTexture2DRHIRef&)TextureRHI;
	}

	const FString& GetCoreObjectName() const;

	bool RegisterResourceHandle_RenderThread(FRHICommandListImmediate& RHICmdList, const FTextureShareCoreResourceRequest& InResourceRequest);
	bool ReleaseTextureShareHandle_RenderThread();

	/** [experimental]
	 * Mark all resources in cache as unused. Later, when the resources are used within the frame, this flag is cleared.
	 */
	virtual void HandleFrameBegin_RenderThread();

	/** [experimental]
	 * Release all unused resources from cache.
	 */
	virtual void HandleFrameEnd_RenderThread();

public:
	virtual void InitRHI(FRHICommandListBase& RHICmdList) override;

	// Returns the width of the texture in pixels.
	virtual uint32 GetSizeX() const override
	{
		return ResourceSettings.Size.X;
	}

	// Returns the height of the texture in pixels.
	virtual uint32 GetSizeY() const override
	{
		return ResourceSettings.Size.Y;
	}

	virtual FString GetFriendlyName() const override
	{
		return TEXT("TextureShareResource");
	}

protected:
	void InitDynamicRHI_Default(FTexture2DRHIRef& OutTextureRHI);
	void InitDynamicRHI_D3D12(FTexture2DRHIRef& OutTextureRHI);

	bool D3D11RegisterResourceHandle_RenderThread(FRHICommandListImmediate& RHICmdList, const FTextureShareCoreResourceRequest& InResourceRequest);
	bool D3D11ReleaseTextureShareHandle_RenderThread();

	bool D3D12RegisterResourceHandle_RenderThread(FRHICommandListImmediate& RHICmdList, const FTextureShareCoreResourceRequest& InResourceRequest);
	bool D3D12ReleaseTextureShareHandle_RenderThread();


#if TEXTURESHARE_VULKAN
	bool VulkanRegisterResourceHandle_RenderThread(FRHICommandListImmediate& RHICmdList, const FTextureShareCoreResourceRequest& InResourceRequest);
	bool VulkanReleaseTextureShareHandle_RenderThread();
#endif

	bool FindCachedSharedResource_RenderThread(void* InNativeResourcePtr, const uint32 InGPUIndex, FTexture2DRHIRef& OutRHIResource) const;
	void AddCachedSharedResource_RenderThread(void* InNativeResourcePtr, const uint32 InGPUIndex, const FTexture2DRHIRef& InRHIResource);
	void CopyToDestResources_RenderThread(FRHICommandListImmediate& RHICmdList, const TArray<FTextureShareExternalTextureRHI>& InDestResources);

private:
	const TSharedRef<ITextureShareCoreObject, ESPMode::ThreadSafe> CoreObject;

	// Resource description
	const FTextureShareCoreResourceDesc ResourceDesc;
	const FTextureShareResourceSettings ResourceSettings;
	TArray<struct FCachedSharedResource> CachedSharedResources;
};
