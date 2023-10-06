// Copyright Epic Games, Inc. All Rights Reserved.

#include "Resources/TextureShareResourcesPool.h"
#include "Containers/TextureShareContainers.h"

#include "RHI.h"
#include "RHICommandList.h"
#include "RenderingThread.h"

//////////////////////////////////////////////////////////////////////////////////////////////
// FTextureShareResourcesPool
//////////////////////////////////////////////////////////////////////////////////////////////
FTextureShareResourcesPool::FTextureShareResourcesPool()
{ }

FTextureShareResourcesPool::~FTextureShareResourcesPool()
{
	Release();
}

//////////////////////////////////////////////////////////////////////////////////////////////
FTextureShareResource* FTextureShareResourcesPool::GetSharedResource_RenderThread(FRHICommandListImmediate& RHICmdList, const TSharedRef<ITextureShareCoreObject, ESPMode::ThreadSafe>& InCoreObject, FRHITexture* InSrcTexture, const FTextureShareCoreResourceRequest& InResourceRequest)
{
	check(IsInRenderingThread());
	check(InSrcTexture);

	const FTextureShareCoreResourceDesc& InResourceDesc = InResourceRequest.ResourceDesc;
	const FTextureShareResourceSettings  InResourceSettings(InResourceRequest, InSrcTexture);

	// Search exist resource
	FTextureShareResource* SharedResource = FindExistTextureShareResource_RenderThread(InResourceDesc);

	if (SharedResource)
	{
		if (SharedResource->GetResourceSettings().Equals(InResourceSettings))
		{
			// Use exist resource
			return SharedResource;
		}

		// Resource settings changed, need to recreate: release old resource
		ReleaseTextureShareResource_RenderThread(SharedResource);
	}

	// Create a new one
	return CreateTextureShareResource_RenderThread(InCoreObject, InResourceDesc, InResourceSettings);
}

FTextureShareResource* FTextureShareResourcesPool::CreateTextureShareResource_RenderThread(const TSharedRef<ITextureShareCoreObject, ESPMode::ThreadSafe>& InCoreObject, const FTextureShareCoreResourceDesc& InResourceDesc, const FTextureShareResourceSettings& InResourceSettings)
{
	if (FTextureShareResource* NewResource = new FTextureShareResource(InCoreObject, InResourceDesc, InResourceSettings))
	{
		TextureResources.Add(NewResource);
		NewResource->InitResource(FRHICommandListImmediate::Get());

		return NewResource;
	}

	return nullptr;
}

FTextureShareResource* FTextureShareResourcesPool::FindExistTextureShareResource_RenderThread(const FTextureShareCoreResourceDesc& InResourceDesc) const
{
	FTextureShareResource* const* Result = TextureResources.FindByPredicate([InResourceDesc](const FTextureShareResource* It) {
		return It && It->GetResourceDesc().EqualsFunc(InResourceDesc);
	});

	return Result ? *Result : nullptr;
}

void FTextureShareResourcesPool::ReleaseTextureShareResource_RenderThread(FTextureShareResource* & InOutResource)
{
	if (InOutResource)
	{
		int32 ResourceIndex = INDEX_NONE;
		if (TextureResources.Find(InOutResource, ResourceIndex))
		{
			TextureResources.RemoveAt(ResourceIndex);
		}

		InOutResource->ReleaseTextureShareHandle_RenderThread();
		InOutResource->ReleaseResource();

		delete InOutResource;
		InOutResource = nullptr;
	}
}

void FTextureShareResourcesPool::Release()
{
	ENQUEUE_RENDER_COMMAND(TextureShare_ResourcesPool)(
		[ReleasedResources = MoveTempIfPossible(TextureResources)](FRHICommandListImmediate& RHICmdList)
	{
		for (FTextureShareResource* ResourceIt : ReleasedResources)
		{
			if (ResourceIt)
			{
				ResourceIt->ReleaseTextureShareHandle_RenderThread();
				ResourceIt->ReleaseResource();
				delete ResourceIt;
			}
		}
	});

	TextureResources.Empty();
}
