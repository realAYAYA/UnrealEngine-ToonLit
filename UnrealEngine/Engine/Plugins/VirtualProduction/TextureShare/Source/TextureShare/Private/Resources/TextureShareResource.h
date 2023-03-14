// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "Resources/TextureShareResourceSettings.h"
#include "Containers/TextureShareCoreContainers_ResourceDesc.h"

#include "RHI.h"
#include "RHIResources.h"
#include "RenderResource.h"

class ITextureShareCoreObject;
struct FTextureShareCoreResourceRequest;

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

	bool RegisterResourceHandle(const FTextureShareCoreResourceRequest& InResourceRequest);
	bool ReleaseTextureShareHandle();

public:
	virtual void InitDynamicRHI() override;

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
	void InitDynamicRHI_TextureResource2D(FTexture2DRHIRef& OutTextureRHI);

	bool D3D11RegisterResourceHandle(const FTextureShareCoreResourceRequest& InResourceRequest);
	bool D3D11ReleaseTextureShareHandle();

	bool D3D12RegisterResourceHandle(const FTextureShareCoreResourceRequest& InResourceRequest);
	bool D3D12ReleaseTextureShareHandle();

#if TEXTURESHARE_VULKAN
	bool VulkanRegisterResourceHandle(const FTextureShareCoreResourceRequest& InResourceRequest);
	bool VulkanReleaseTextureShareHandle();
#endif

private:
	const TSharedRef<ITextureShareCoreObject, ESPMode::ThreadSafe> CoreObject;

	// Resource description
	const FTextureShareCoreResourceDesc ResourceDesc;

	// Resource settings
	const FTextureShareResourceSettings ResourceSettings;
};
