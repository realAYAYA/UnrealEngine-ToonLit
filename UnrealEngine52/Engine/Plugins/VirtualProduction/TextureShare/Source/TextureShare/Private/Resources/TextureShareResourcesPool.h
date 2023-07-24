// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"

#include "Containers/TextureShareCoreEnums.h"
#include "Resources/TextureShareResource.h"

class ITextureShareCoreObject;

/**
 * TextureShare RHI resources pool
 */
class FTextureShareResourcesPool
{
public:
	FTextureShareResourcesPool();
	~FTextureShareResourcesPool();

	void Release();

public:
	FTextureShareResource* GetSharedResource_RenderThread(FRHICommandListImmediate& RHICmdList, const TSharedRef<ITextureShareCoreObject, ESPMode::ThreadSafe>& InCoreObject, FRHITexture* InSrcTexture, const FTextureShareCoreResourceRequest& InResourceRequest);

	bool IsRHICommandsListChanged_RenderThread() const
	{
		check(IsInRenderingThread());
		return bIsRHICommandsListChanged;
	}

	void ClearFlagRHICommandsListChanged_RenderThread()
	{
		check(IsInRenderingThread());
		bIsRHICommandsListChanged = false;
	}

private:
	FTextureShareResource* FindExistTextureShareResource_RenderThread(const FTextureShareCoreResourceDesc& InResourceDesc) const;
	FTextureShareResource* CreateTextureShareResource_RenderThread(const TSharedRef<ITextureShareCoreObject, ESPMode::ThreadSafe>& InCoreObject, const FTextureShareCoreResourceDesc& InResourceDesc, const FTextureShareResourceSettings& InResourceSettings);
	void ReleaseTextureShareResource_RenderThread(FTextureShareResource* &InOutResource);

private:
	TArray<FTextureShareResource*> TextureResources;

	// when RHI changed inside proxy function then value will be true
	bool bIsRHICommandsListChanged = false;

};
