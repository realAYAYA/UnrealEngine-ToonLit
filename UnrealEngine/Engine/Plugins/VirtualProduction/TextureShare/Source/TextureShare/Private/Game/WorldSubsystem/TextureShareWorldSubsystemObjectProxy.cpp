// Copyright Epic Games, Inc. All Rights Reserved.

#include "Game/WorldSubsystem/TextureShareWorldSubsystemObjectProxy.h"
#include "Game/ViewExtension/TextureShareSceneViewExtension.h"
#include "Module/TextureShareLog.h"
#include "Misc/TextureShareStrings.h"

#include "ITextureShare.h"
#include "ITextureShareObject.h"
#include "ITextureShareObjectProxy.h"
#include "ITextureShareCoreObject.h"

#include "Blueprints/TextureShareBlueprintContainers.h"

//////////////////////////////////////////////////////////////////////////////////////////////
struct FReceiveTexture
{
	FReceiveTexture(const FString& InTextureId, FTextureRenderTargetResource* InTextureResource)
		: Name(InTextureId), Texture(InTextureResource)
	{ }

	const FString Name;
	FTextureRenderTargetResource* const Texture;
};

struct FSendTexture
{
	FSendTexture(const FString& InTextureId, FTextureResource* InTextureResource)
		: Name(InTextureId), Texture(InTextureResource)
	{ }

	const FString Name;
	FTextureResource* const Texture;
};

struct FProxyResourcesData
{
	TArray<FReceiveTexture> Receive;
	TArray<FSendTexture>    Send;

public:
	FProxyResourcesData(const UTextureShareObject* InTextureShareObject)
	{
		if (InTextureShareObject)
		{
			const FTextureShareTexturesDesc& InTextures = InTextureShareObject->Textures;

			// Get any possible send resources
			for (const FTextureShareSendTextureDesc& It : InTextures.SendTextures)
			{
				if (FTextureResource* SendResource = It.Texture ? It.Texture->GetResource() : nullptr)
				{
					Send.Add(FSendTexture(It.Name, SendResource));
				}
			}

			// and receive
			for (const FTextureShareReceiveTextureDesc& It : InTextures.ReceiveTextures)
			{
				if (FTextureRenderTargetResource* ReceiveTexture = It.Texture ? It.Texture->GameThread_GetRenderTargetResource() : nullptr)
				{
					Receive.Add(FReceiveTexture(It.Name, ReceiveTexture));
				}
			}
		}
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////
// FTextureShareWorldSubsystemObjectProxy
//////////////////////////////////////////////////////////////////////////////////////////////
FTextureShareWorldSubsystemObjectProxy::FTextureShareWorldSubsystemObjectProxy(const TSharedPtr<ITextureShareObjectProxy, ESPMode::ThreadSafe>& InObjectProxy)
	: ObjectProxy(InObjectProxy)
{ }

bool FTextureShareWorldSubsystemObjectProxy::Update(const UTextureShareObject* InTextureShareObject)
{
	// Updated sceneproxy on renderthread
	ENQUEUE_RENDER_COMMAND(TextureShare_UpdateObjectProxy)(
		[ObjectProxy = ObjectProxy, ProxyData = MakeShared<FProxyResourcesData>(InTextureShareObject)](FRHICommandListImmediate& RHICmdList)
	{
		FTextureShareWorldSubsystemObjectProxy ProxyAPI(ObjectProxy);
		ProxyAPI.Update_RenderThread(RHICmdList, ProxyData);
	});

	return true;
};

void FTextureShareWorldSubsystemObjectProxy::Update_RenderThread(FRHICommandListImmediate& RHICmdList, TSharedPtr<FProxyResourcesData> ProxyData)
{
	check(IsInRenderingThread());

	if (ObjectProxy.IsValid())
	{
		const TSharedPtr<FTextureShareSceneViewExtension, ESPMode::ThreadSafe> ViewExtension = ObjectProxy->GetViewExtension_RenderThread();

		if (ViewExtension.IsValid())
		{
			// Find any backbuffer resource request
			const FTextureShareCoreResourceRequest* AnyBackbufferResourceRequest = ObjectProxy->GetData_RenderThread().FindResourceRequest(FTextureShareCoreResourceDesc(TextureShareStrings::SceneTextures::Backbuffer, ETextureShareTextureOp::Undefined));
			const bool bBackbufferShared = AnyBackbufferResourceRequest != nullptr;

			TFunctionTextureShareViewExtension PreRenderViewFamilyFunction = [ProxyData](FRHICommandListImmediate& RHICmdList, FTextureShareSceneViewExtension& InViewExtension)
			{
				TSharedPtr<ITextureShareObjectProxy, ESPMode::ThreadSafe> ObjectProxy = InViewExtension.GetObjectProxy();
				if (ObjectProxy.IsValid())
				{
					ObjectProxy->BeginFrameSync_RenderThread(RHICmdList);
				}
			};

			TFunctionTextureShareViewExtension PostRenderViewFamilyFunction = [ProxyData, bBackbufferShared](FRHICommandListImmediate& RHICmdList, FTextureShareSceneViewExtension& InViewExtension)
			{
				// Share user resources
				TSharedPtr<ITextureShareObjectProxy, ESPMode::ThreadSafe> ObjectProxy = InViewExtension.GetObjectProxy();
				if (ObjectProxy.IsValid()
					&& ObjectProxy->IsFrameSyncActive_RenderThread())
				{
					FTextureShareWorldSubsystemObjectProxy ProxyAPI(ObjectProxy);

					ProxyAPI.UpdateResources_RenderThread(RHICmdList, ProxyData);

					if (bBackbufferShared)
					{
						ObjectProxy->FrameSync_RenderThread(RHICmdList, ETextureShareSyncStep::FrameProxyPreRenderEnd);
					}
					else
					{
						ObjectProxy->EndFrameSync_RenderThread(RHICmdList);
					}
				}
			};

			TFunctionTextureShareOnBackBufferReadyToPresent OnBackBufferReadyToPresentFunction = [ProxyData, bBackbufferShared](FRHICommandListImmediate& RHICmdList, FTextureShareSceneViewExtension& InViewExtension, const FTexture2DRHIRef& InBackbuffer)
			{
				// Share backbuffer
				if (bBackbufferShared)
				{
					TSharedPtr<ITextureShareObjectProxy, ESPMode::ThreadSafe> ObjectProxy = InViewExtension.GetObjectProxy();
					if (ObjectProxy.IsValid()
						&& ObjectProxy->IsFrameSyncActive_RenderThread())
					{
						FTextureShareWorldSubsystemObjectProxy ProxyAPI(ObjectProxy);

						ProxyAPI.UpdateFrameProxyBackbuffer_RenderThread(RHICmdList, InBackbuffer);

						ObjectProxy->EndFrameSync_RenderThread(RHICmdList);
					}
				}
			};

			// Setup sync logic for each frame. This is because the sync logic can be changed at runtime.
			// As a result, the minimum number of IPC locks will be used.
			// For example, when a remote process requests a back buffer, the functor logic will be changed. and contraverse.
			ViewExtension->SetPreRenderViewFamilyFunction(&PreRenderViewFamilyFunction);
			ViewExtension->SetPostRenderViewFamilyFunction(&PostRenderViewFamilyFunction);
			ViewExtension->SetOnBackBufferReadyToPresentFunction(&OnBackBufferReadyToPresentFunction);

			// Enable receive for scene textures (single viewport case)
			ViewExtension->SetEnableObjectProxySync(true);
		}
		else
		{
			// Just share custom resources
			if (ObjectProxy->BeginFrameSync_RenderThread(RHICmdList)
				&& ObjectProxy->IsFrameSyncActive_RenderThread())
			{
				UpdateResources_RenderThread(RHICmdList, ProxyData);
				ObjectProxy->EndFrameSync_RenderThread(RHICmdList);
			}
		}
	}
}

bool FTextureShareWorldSubsystemObjectProxy::UpdateResources_RenderThread(FRHICommandListImmediate& RHICmdList, TSharedPtr<FProxyResourcesData> ProxyData)
{
	if (ObjectProxy.IsValid()
		&& ProxyData.IsValid()
		&& ObjectProxy->IsFrameSyncActive_RenderThread())
	{
		// mgpu is currently not handled by this logic
		const int32 GPUIndex = -1;

		// Send custom textures
		for (const FSendTexture& SendIt : ProxyData->Send)
		{
			if (SendIt.Texture)
			{
				// Send if remote process request to read this texture
				ObjectProxy->ShareResource_RenderThread(RHICmdList, FTextureShareCoreResourceDesc(SendIt.Name.ToLower(), ETextureShareTextureOp::Read), SendIt.Texture->GetTexture2DRHI(), GPUIndex);
			}
		}
		// and receive custom textures
		for (const FReceiveTexture& ReceiveIt : ProxyData->Receive)
		{
			if (ReceiveIt.Texture)
			{
				// Receive if remote process request to write this texture
				ObjectProxy->ShareResource_RenderThread(RHICmdList, FTextureShareCoreResourceDesc(ReceiveIt.Name.ToLower(), ETextureShareTextureOp::Write, ETextureShareSyncStep::FrameProxyPreRenderEnd), ReceiveIt.Texture->GetTexture2DRHI(), GPUIndex);
			}
		}

		return true;
	}

	return false;
}

bool FTextureShareWorldSubsystemObjectProxy::UpdateFrameProxyBackbuffer_RenderThread(FRHICommandListImmediate& RHICmdList, const FTexture2DRHIRef& InBackbuffer)
{
	if (ObjectProxy.IsValid()
		&& InBackbuffer.IsValid()
		&& ObjectProxy->IsFrameSyncActive_RenderThread())
	{
		// mgpu is currently not handled by this logic
		const int32 GPUIndex = -1;

		// Send if remote process request to read this texture
		ObjectProxy->ShareResource_RenderThread(RHICmdList, FTextureShareCoreResourceDesc(TextureShareStrings::SceneTextures::Backbuffer, ETextureShareTextureOp::Read), InBackbuffer.GetReference(), GPUIndex);
		// Receive if remote process request to write this texture
		ObjectProxy->ShareResource_RenderThread(RHICmdList, FTextureShareCoreResourceDesc(TextureShareStrings::SceneTextures::Backbuffer, ETextureShareTextureOp::Write, ETextureShareSyncStep::FrameProxyBackBufferReadyToPresentEnd), InBackbuffer.GetReference(), GPUIndex);

		return true;
	}

	return false;
}
