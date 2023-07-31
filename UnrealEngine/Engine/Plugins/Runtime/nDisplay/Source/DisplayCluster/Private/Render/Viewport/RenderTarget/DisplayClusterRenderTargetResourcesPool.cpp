// Copyright Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterRenderTargetResourcesPool.h"
#include "Render/Viewport/RenderFrame/DisplayClusterRenderFrameSettings.h"
#include "Render/Viewport/DisplayClusterViewportHelpers.h"
#include "Render/Viewport/DisplayClusterViewportManagerProxy.h"

#include "RHICommandList.h"
#include "Engine/RendererSettings.h"
#include "RenderingThread.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////
namespace DisplayClusterRenderTargetResourcesPool
{
	static void ImplInitializeViewportResourcesRHI(const TArrayView<FDisplayClusterViewportResource*>& InResources)
	{
		TArray<FDisplayClusterViewportResource*> ResourcesForRenderThread(InResources);

		ENQUEUE_RENDER_COMMAND(DisplayCluster_InitializeViewportResourcesRHI)(
			[NewResources = std::move(ResourcesForRenderThread)](FRHICommandListImmediate& RHICmdList)
		{
			for (FDisplayClusterViewportResource* ResourceIt : NewResources)
			{
				ResourceIt->InitResource();
			}
		});
	}

	static void ImplReleaseViewportResourcesRHI(FDisplayClusterViewportManagerProxy* InViewportManagerProxy, const TArrayView<FDisplayClusterViewportResource*>& InResources)
	{
		if (InViewportManagerProxy != nullptr && !InResources.IsEmpty())
		{
		TArray<FDisplayClusterViewportResource*> ResourcesForRenderThread(InResources);

		ENQUEUE_RENDER_COMMAND(DisplayCluster_ReleaseViewportResourcesRHI)(
				[InViewportManagerProxy = InViewportManagerProxy->AsShared(), ReleasedResources = std::move(ResourcesForRenderThread)](FRHICommandListImmediate& RHICmdList)
			{
				for (FDisplayClusterViewportResource* DeletedResourcePtrIt : ReleasedResources)
				{
					InViewportManagerProxy->DeleteResource_RenderThread(DeletedResourcePtrIt);
				}
			});
		}
	}

	static FDisplayClusterViewportResourceSettings* ImplCreateViewportResourceSettings(const FDisplayClusterRenderFrameSettings& InRenderFrameSettings, FViewport* InViewport)
	{
		FDisplayClusterViewportResourceSettings Result;

		if (InViewport != nullptr)
		{
			FRHITexture2D* ViewportTexture = InViewport->GetRenderTargetTexture();
			if (ViewportTexture != nullptr)
			{
				EPixelFormat Format = ViewportTexture->GetFormat();
				bool bShouldUseSRGB = EnumHasAnyFlags(ViewportTexture->GetFlags(), TexCreate_SRGB);
				float Gamma = InViewport->GetDisplayGamma();

				return new FDisplayClusterViewportResourceSettings(InRenderFrameSettings.ClusterNodeId, Format, bShouldUseSRGB, Gamma);
			}
		}

#if WITH_EDITOR
		EPixelFormat PreviewPixelFormat;
		float PreviewDisplayGamma = 1.f;
		bool bPreviewSRGB = true;
		DisplayClusterViewportHelpers::GetPreviewRenderTargetDesc_Editor(InRenderFrameSettings, PreviewPixelFormat, PreviewDisplayGamma, bPreviewSRGB);

		return new FDisplayClusterViewportResourceSettings(InRenderFrameSettings.ClusterNodeId, PreviewPixelFormat, bPreviewSRGB, PreviewDisplayGamma);
#else
		return nullptr;
#endif
	}
};

using namespace DisplayClusterRenderTargetResourcesPool;

////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// FDisplayClusterRenderTargetResourcesPool
////////////////////////////////////////////////////////////////////////////////////////////////////////////
FDisplayClusterRenderTargetResourcesPool::FDisplayClusterRenderTargetResourcesPool(FDisplayClusterViewportManagerProxy* InViewportManagerProxy)
	: ViewportManagerProxy(InViewportManagerProxy)
{ }

FDisplayClusterRenderTargetResourcesPool::~FDisplayClusterRenderTargetResourcesPool()
{
	Release();
}

void FDisplayClusterRenderTargetResourcesPool::Release()
{
	// Release all resources
	ImplReleaseResources<FDisplayClusterViewportRenderTargetResource>(RenderTargetResources);
	ImplReleaseResources<FDisplayClusterViewportTextureResource>(TextureResources);
}

template <typename TViewportResourceType>
void FDisplayClusterRenderTargetResourcesPool::ImplBeginReallocateResources(TArray<TViewportResourceType*>& InOutViewportResources)
{
	check(ResourceSettings != nullptr);

	// Mark cluster node resources as unused
	for (TViewportResourceType* ResourceIt : InOutViewportResources)
	{
		if (ResourceIt != nullptr && ResourceIt->GetViewportResourceState(EDisplayClusterViewportResourceState::DisableReallocate) == false)
		{
			if (const bool bIsCurrentClusterNodeResource = ResourceIt->GetResourceSettingsConstRef().IsClusterNodeNameEqual(*ResourceSettings))
			{
				ResourceIt->RaiseViewportResourceState(EDisplayClusterViewportResourceState::Unused);
			}
			else
			{
				ResourceIt->RaiseViewportResourceState(EDisplayClusterViewportResourceState::DisableReallocate);
			}
		}
	}
}

template <typename TViewportResourceType>
void FDisplayClusterRenderTargetResourcesPool::ImplFinishReallocateResources(TArray<TViewportResourceType*>& InOutViewportResources)
{
	// Collect new and unused resources 
	TArray<TViewportResourceType*> NewResources;
	TArray<TViewportResourceType*> UnusedResources;

	for (TViewportResourceType* ResourceIt : InOutViewportResources)
	{
		if (ResourceIt != nullptr)
		{
			check(ResourceIt->GetViewportResourceState(EDisplayClusterViewportResourceState::Deleted) == false);

			if (ResourceIt->GetViewportResourceState(EDisplayClusterViewportResourceState::Initialized) == false)
			{
				// Collect new resources
				NewResources.Add(ResourceIt);
				ResourceIt->RaiseViewportResourceState(EDisplayClusterViewportResourceState::Initialized);
			}
			else if (ResourceIt->GetViewportResourceState(EDisplayClusterViewportResourceState::Unused) == true)
			{
				// Collect unused resources
				UnusedResources.Add(ResourceIt);
				ResourceIt->RaiseViewportResourceState(EDisplayClusterViewportResourceState::Deleted);
			}

			// Clear 'DisableReallocate' at frame end
			ResourceIt->ClearViewportResourceState(EDisplayClusterViewportResourceState::DisableReallocate);
		}
	}

	// Init RHI for new resources
	if (NewResources.Num() > 0)
	{
		// Send to rendering thread
		ImplInitializeViewportResourcesRHI(TArrayView<FDisplayClusterViewportResource*>((FDisplayClusterViewportResource**)(NewResources.GetData()), NewResources.Num()));
	}

	// Remove unused resources
	if (UnusedResources.Num() > 0)
	{
		// Remove GameThread resource references
		for (TViewportResourceType* ResourceIt : UnusedResources)
		{
			int ResourceIndex = INDEX_NONE;
			while (InOutViewportResources.Find(ResourceIt, ResourceIndex))
			{
				InOutViewportResources.RemoveAt(ResourceIndex);
			}
		}

		// Send to released resources to rendering thread
		ImplReleaseViewportResourcesRHI(ViewportManagerProxy, TArrayView<FDisplayClusterViewportResource*>((FDisplayClusterViewportResource**)(UnusedResources.GetData()), UnusedResources.Num()));
	}
}

template <typename TViewportResourceType>
void FDisplayClusterRenderTargetResourcesPool::ImplReleaseResources(TArray<TViewportResourceType*>& InOutViewportResources)
{
	// Send all resources to rendering thread
	ImplReleaseViewportResourcesRHI(ViewportManagerProxy, TArray<FDisplayClusterViewportResource*>((FDisplayClusterViewportResource**)(InOutViewportResources.GetData()), InOutViewportResources.Num()));

	// Reset refs on game thread too
	InOutViewportResources.Empty();
}

template <typename TViewportResourceType>
TViewportResourceType* FDisplayClusterRenderTargetResourcesPool::ImplAllocateResource(TArray<TViewportResourceType*>& InOutViewportResources, const FDisplayClusterViewportResourceSettings& InSettings)
{
	if (!DisplayClusterViewportHelpers::IsValidTextureSize(InSettings.Size))
	{
		return nullptr;
	}

	// Unused resources marked for current cluster node
	auto ExistAndUnusedResourceIndex = InOutViewportResources.IndexOfByPredicate([InSettings](const TViewportResourceType* ResourceIt)
	{
		if (ResourceIt != nullptr && ResourceIt->GetViewportResourceState(EDisplayClusterViewportResourceState::Unused) == true)
		{
			return ResourceIt->GetResourceSettingsConstRef().IsResourceSettingsEqual(InSettings);
		}

		return false;
	});

	if (ExistAndUnusedResourceIndex != INDEX_NONE)
	{
		// Use exist resource again
		TViewportResourceType* ExistResource = InOutViewportResources[ExistAndUnusedResourceIndex];

		// Clear unused state for re-used resource
		ExistResource->ClearViewportResourceState(EDisplayClusterViewportResourceState::Unused);

		return ExistResource;
	}

	// Create new resource:
	TViewportResourceType* NewResource = new TViewportResourceType(InSettings);
	InOutViewportResources.Add(NewResource);

	return NewResource;
}

bool FDisplayClusterRenderTargetResourcesPool::BeginReallocateResources(const FDisplayClusterRenderFrameSettings& InRenderFrameSettings, FViewport* InViewport)
{
	check(ResourceSettings == nullptr);

	// Initialize settings for new render frame
	ResourceSettings = ImplCreateViewportResourceSettings(InRenderFrameSettings, InViewport);

	if(ResourceSettings != nullptr)
	{
		// Begin reallocate resources for current cluster node
		ImplBeginReallocateResources<FDisplayClusterViewportRenderTargetResource>(RenderTargetResources);
		ImplBeginReallocateResources<FDisplayClusterViewportTextureResource>(TextureResources);

		return true;
	}

	return false;
}

void FDisplayClusterRenderTargetResourcesPool::FinishReallocateResources()
{
	if (ResourceSettings != nullptr)
	{
		// Finish reallocate resources for current cluster node
		ImplFinishReallocateResources<FDisplayClusterViewportRenderTargetResource>(RenderTargetResources);
		ImplFinishReallocateResources<FDisplayClusterViewportTextureResource>(TextureResources);

		delete ResourceSettings;
		ResourceSettings = nullptr;
	}
}

FDisplayClusterViewportRenderTargetResource* FDisplayClusterRenderTargetResourcesPool::AllocateRenderTargetResource(const FIntPoint& InSize, enum EPixelFormat CustomPixelFormat)
{
	check(ResourceSettings != nullptr);

	return ImplAllocateResource<FDisplayClusterViewportRenderTargetResource>(RenderTargetResources, FDisplayClusterViewportResourceSettings(*ResourceSettings, InSize, CustomPixelFormat));
}

FDisplayClusterViewportTextureResource* FDisplayClusterRenderTargetResourcesPool::AllocateTextureResource(const FIntPoint& InSize, bool bIsRenderTargetable, enum EPixelFormat CustomPixelFormat, int32 InNumMips)
{
	check(ResourceSettings != nullptr);

	return ImplAllocateResource<FDisplayClusterViewportTextureResource>(TextureResources, FDisplayClusterViewportResourceSettings(*ResourceSettings, InSize, CustomPixelFormat, bIsRenderTargetable, InNumMips));
}
