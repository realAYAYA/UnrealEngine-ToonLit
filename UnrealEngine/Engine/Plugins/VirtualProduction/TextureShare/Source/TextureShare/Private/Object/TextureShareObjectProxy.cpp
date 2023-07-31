// Copyright Epic Games, Inc. All Rights Reserved.

#include "Object/TextureShareObjectProxy.h"
#include "Object/TextureShareObject.h"

#include "Game/ViewExtension/TextureShareSceneViewExtension.h"
#include "Resources/TextureShareResourcesProxy.h"

#include "Module/TextureShareLog.h"
#include "Misc/TextureShareStrings.h"

#include "ITextureShareCallbacks.h"
#include "ITextureShareCore.h"

#include "RenderGraphUtils.h"

//////////////////////////////////////////////////////////////////////////////////////////////
// FTextureShareObjectProxy
//////////////////////////////////////////////////////////////////////////////////////////////
FTextureShareObjectProxy::FTextureShareObjectProxy(const TSharedRef<ITextureShareCoreObject, ESPMode::ThreadSafe>& InCoreObject)
	: CoreObject(InCoreObject)
	, TextureShareData(MakeShared<FTextureShareData, ESPMode::ThreadSafe>())
{ }

FTextureShareObjectProxy::~FTextureShareObjectProxy()
{
	EndSession_RenderThread();

	// Finally release CoreObject
	CoreObject->RemoveObject();
}

//////////////////////////////////////////////////////////////////////////////////////////////
const FString& FTextureShareObjectProxy::GetName_RenderThread() const
{
	check(IsInRenderingThread());

	return CoreObject->GetName();
}

const FTextureShareCoreObjectDesc& FTextureShareObjectProxy::GetObjectDesc_RenderThread() const
{
	check(IsInRenderingThread());

	return CoreObject->GetObjectDesc();
}

bool FTextureShareObjectProxy::IsActive_RenderThread() const
{
	check(IsInRenderingThread());

	return CoreObject->IsActive();
}

//////////////////////////////////////////////////////////////////////////////////////////////
bool FTextureShareObjectProxy::IsFrameSyncActive_RenderThread() const
{
	check(IsInRenderingThread());

	return bSessionStarted && bFrameProxySyncActive && CoreObject->IsFrameSyncActive();
}

bool FTextureShareObjectProxy::BeginFrameSync_RenderThread(FRHICommandListImmediate& RHICmdList) const
{
	check(IsInRenderingThread());

	if (CoreObject->IsBeginFrameSyncActive_RenderThread() && CoreObject->LockThreadMutex(ETextureShareThreadMutex::RenderingThread))
	{
		if (CoreObject->BeginFrameSync_RenderThread())
		{
			// update frame markers from game thread data
			FTextureShareCoreProxyData& CoreProxyDataRef = CoreObject->GetProxyData_RenderThread();
				
			// Copy frame marker from game thread
			CoreProxyDataRef.FrameMarker = TextureShareData->ObjectData.FrameMarker;

			// Copy the frame markers from the objects saved at the end of the game stream.
			CoreProxyDataRef.RemoteFrameMarkers.Empty();

			// Update remote frame markers
			for (const FTextureShareCoreObjectData& ObjectDataIt : TextureShareData->ReceivedObjectsData)
			{
				CoreProxyDataRef.RemoteFrameMarkers.Add(FTextureShareCoreObjectFrameMarker(ObjectDataIt.Desc, ObjectDataIt.Data.FrameMarker));
			}

			bFrameProxySyncActive = true;

			if (ITextureShareCallbacks::Get().OnTextureShareBeginFrameSyncEvent_RenderThread().IsBound())
			{
				ITextureShareCallbacks::Get().OnTextureShareBeginFrameSyncEvent_RenderThread().Broadcast(RHICmdList, *this);
			}

			return true;
		}
	}

	return false;
}

bool FTextureShareObjectProxy::EndFrameSync_RenderThread(FRHICommandListImmediate& RHICmdList) const
{
	check(IsInRenderingThread());

	// Always force RHI&sync flush for render proxy
	FrameSync_RenderThread(RHICmdList, ETextureShareSyncStep::FrameProxyFlush);

	const bool bResult = CoreObject->EndFrameSync_RenderThread();

	bFrameProxySyncActive = false;

	if (ITextureShareCallbacks::Get().OnTextureShareEndFrameSyncEvent_RenderThread().IsBound())
	{
		ITextureShareCallbacks::Get().OnTextureShareEndFrameSyncEvent_RenderThread().Broadcast(RHICmdList, *this);
	}

	CoreObject->UnlockThreadMutex(ETextureShareThreadMutex::GameThread);

	return bResult;
}

DECLARE_GPU_STAT_NAMED(TextureShareObjectProxyFrameSync, TEXT("TextureShare::FrameSync_RenderThread"));

bool FTextureShareObjectProxy::FrameSync_RenderThread(FRHICommandListImmediate& RHICmdList, const ETextureShareSyncStep InSyncStep) const
{
	check(IsInRenderingThread());

	SCOPED_GPU_STAT(RHICmdList, TextureShareObjectProxyFrameSync);
	SCOPED_DRAW_EVENT(RHICmdList, TextureShareObjectProxyFrameSync);

	TRACE_CPUPROFILER_EVENT_SCOPE(TextureShare::FrameSync_RenderThread);

	if (IsFrameSyncActive_RenderThread())
	{
		if (ResourcesProxy.IsValid())
		{
			// step 1: flush RHI thread if needed to be sure about surfaces ready for sharing
			ResourcesProxy->RHIThreadFlush_RenderThread(RHICmdList);

			// step 2: support mGPU for sender
			ResourcesProxy->RunCrossGPUTransfer_RenderThread(ECrossGPUTransferType::BeforeSync, RHICmdList, InSyncStep);

			// step 3: update shared resources handles and register it
			ResourcesProxy->RunRegisterResourceHandles_RenderThread();
		}

		// synchronize data between processes
		if (CoreObject->FrameSync_RenderThread(InSyncStep))
		{
			if (ResourcesProxy.IsValid())
			{
				// step 4: support mGPU for received textures
				ResourcesProxy->RunCrossGPUTransfer_RenderThread(ECrossGPUTransferType::AfterSync, RHICmdList, InSyncStep);

				// step 5: copy received textures
				ResourcesProxy->RunReceiveResources_RenderThread(RHICmdList, InSyncStep);
			}

			if (ITextureShareCallbacks::Get().OnTextureShareFrameSyncEvent_RenderThread().IsBound())
			{
				ITextureShareCallbacks::Get().OnTextureShareFrameSyncEvent_RenderThread().Broadcast(RHICmdList, *this, InSyncStep);
			}

			return true;
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////////////////////////
const FTextureShareData& FTextureShareObjectProxy::GetData_RenderThread() const
{
	check(IsInRenderingThread());

	return *TextureShareData;
}

const TSharedPtr<FTextureShareSceneViewExtension, ESPMode::ThreadSafe>& FTextureShareObjectProxy::GetViewExtension_RenderThread() const
{
	check(IsInRenderingThread());

	return ViewExtension;
}

FTextureShareCoreProxyData& FTextureShareObjectProxy::GetCoreProxyData_RenderThread()
{
	check(IsInRenderingThread());

	return CoreObject->GetProxyData_RenderThread();
}

const FTextureShareCoreProxyData& FTextureShareObjectProxy::GetCoreProxyData_RenderThread() const
{
	check(IsInRenderingThread());

	return CoreObject->GetProxyData_RenderThread();
}

const TArray<FTextureShareCoreObjectProxyData>& FTextureShareObjectProxy::GetReceivedCoreObjectProxyData_RenderThread() const
{
	check(IsInRenderingThread());

	return CoreObject->GetReceivedProxyData_RenderThread();
}

//////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SHADER_PARAMETER_STRUCT(FSendTextureParameters, )
RDG_TEXTURE_ACCESS(Texture, ERHIAccess::CopySrc)
END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(FReceiveTextureParameters, )
RDG_TEXTURE_ACCESS(Texture, ERHIAccess::CopyDest)
END_SHADER_PARAMETER_STRUCT()

bool FTextureShareObjectProxy::ShareResource_RenderThread(FRDGBuilder& GraphBuilder, const FTextureShareCoreResourceDesc& InResourceDesc, const FRDGTextureRef& InTextureRef, const int32 InTextureGPUIndex, const FIntRect* InTextureRect) const
{
	if (HasBeenProduced(InTextureRef))
	{
		FIntRect InViewRect = (InTextureRect) ? *InTextureRect : FIntRect();

		switch (InResourceDesc.OperationType)
		{
		case ETextureShareTextureOp::Read:
		{
			FSendTextureParameters* PassParameters = GraphBuilder.AllocParameters<FSendTextureParameters>();
			PassParameters->Texture = InTextureRef;

			GraphBuilder.AddPass(
				RDG_EVENT_NAME("TextureShare_SendRDGTexture_%s", *InResourceDesc.ResourceName),
				PassParameters,
				ERDGPassFlags::Copy | ERDGPassFlags::NeverCull,
				[ObjectProxy = SharedThis(this), SyncStep = InResourceDesc.SyncStep, InResourceDesc, InTextureRef, InTextureGPUIndex, InViewRect](FRHICommandListImmediate& RHICmdList)
				{
					ObjectProxy->ShareResource_RenderThread(RHICmdList, InResourceDesc, InTextureRef->GetRHI(), InTextureGPUIndex, &InViewRect);
				});

			return true;
		}
		break;

		case ETextureShareTextureOp::Write:
		{
			FReceiveTextureParameters* PassParameters = GraphBuilder.AllocParameters<FReceiveTextureParameters>();
			PassParameters->Texture = InTextureRef;

			GraphBuilder.AddPass(
				RDG_EVENT_NAME("TextureShare_ReceiveRDGTexture_%s", *InResourceDesc.ResourceName),
				PassParameters,
				ERDGPassFlags::Copy | ERDGPassFlags::NeverCull,
				[ObjectProxy = SharedThis(this), InTextureRef, InTextureGPUIndex, InResourceDesc, InViewRect](FRHICommandListImmediate& RHICmdList)
				{
					ObjectProxy->ShareResource_RenderThread(RHICmdList, InResourceDesc, InTextureRef->GetRHI(), InTextureGPUIndex, &InViewRect);
				});

			return true;
		}
		break;

		default:
			break;
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////////////////////////
DECLARE_GPU_STAT_NAMED(TextureShareObjectProxyShareResource, TEXT("TextureShare::ShareResource_RenderThread"));

bool FTextureShareObjectProxy::ShareResource_RenderThread(FRHICommandListImmediate& RHICmdList, const FTextureShareCoreResourceDesc& InResourceDesc, FRHITexture* InTexture, const int32 InTextureGPUIndex, const FIntRect* InTextureRect) const
{
	check(IsInRenderingThread());

	if (InTexture && InTexture->IsValid() && ResourcesProxy.IsValid() && IsFrameSyncActive_RenderThread())
	{
		// Share only requested resources
		if (const FTextureShareCoreResourceRequest* ExistResourceRequest = GetData_RenderThread().FindResourceRequest(InResourceDesc))
		{
			if (FTextureShareResource* SharedResource = ResourcesProxy->GetSharedTexture_RenderThread(RHICmdList, CoreObject, InTexture, *ExistResourceRequest))
			{

				SCOPED_GPU_STAT(RHICmdList, TextureShareObjectProxyShareResource);
				SCOPED_DRAW_EVENT(RHICmdList, TextureShareObjectProxyShareResource);

				TRACE_CPUPROFILER_EVENT_SCOPE(TextureShare::ShareResource_RenderThread);

				switch (ExistResourceRequest->ResourceDesc.OperationType)
				{
					// Remote process request read texture, send it
				case ETextureShareTextureOp::Read:
					// Copy SrcTexture to DstSharedTextureShare immediatelly
					if (ResourcesProxy->WriteToShareTexture_RenderThread(RHICmdList, InTexture, InTextureRect, SharedResource))
					{
						// deferred register
						ResourcesProxy->PushRegisterResource_RenderThread(*ExistResourceRequest, SharedResource);

						// register shared RHI resource for mgpu transfer, before sync
						ResourcesProxy->PushCrossGPUTransfer_RenderThread(ECrossGPUTransferType::BeforeSync, SharedResource, InTextureGPUIndex, ExistResourceRequest->GPUIndex);

						return true;
					}
					break;

					// Remote process request write texture, receive it
				case ETextureShareTextureOp::Write:
					// deferred register for receive
					ResourcesProxy->PushRegisterResource_RenderThread(*ExistResourceRequest, SharedResource);

					// register shared RHI resource for mgpu transfer, post-sync
					ResourcesProxy->PushCrossGPUTransfer_RenderThread(ECrossGPUTransferType::AfterSync, SharedResource, ExistResourceRequest->GPUIndex, InTextureGPUIndex);

					// register requestes at this point and updated latter
					ResourcesProxy->PushReceiveResource_RenderThread(*ExistResourceRequest, SharedResource, InTexture, InTextureRect);

					return true;

				default:
					break;
				}
			}
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////////////////////////
bool FTextureShareObjectProxy::BeginSession_RenderThread()
{
	if (!bSessionStarted)
	{
		ResourcesProxy = MakeUnique<FTextureShareResourcesProxy>();

		bSessionStarted = true;

		return true;
	}

	return false;
}

bool FTextureShareObjectProxy::EndSession_RenderThread()
{
	if (bSessionStarted)
	{
		bSessionStarted = false;

		ResourcesProxy.Reset();

		return CoreObject->EndSession();
	}

	return false;
}

void FTextureShareObjectProxy::HandleNewFrame_RenderThread(const TSharedRef<FTextureShareData, ESPMode::ThreadSafe>& InTextureShareData, const TSharedPtr<FTextureShareSceneViewExtension, ESPMode::ThreadSafe>& InViewExtension)
{
	check(IsInRenderingThread());

	// Assign new frame data sent from game thread
	TextureShareData = InTextureShareData;

	ViewExtension = InViewExtension;

	// Mark RHI thread as dirty for flush
	if (ResourcesProxy.IsValid())
	{
		// Release unused stuff from old frame (sync lost purpose)
		ResourcesProxy->Empty();

		// Force flush RHI before any actions
		ResourcesProxy->ForceRHIFlushFlush_RenderThread();
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////
void FTextureShareObjectProxy::BeginSession_GameThread(const FTextureShareObject& In)
{
	ENQUEUE_RENDER_COMMAND(TextureShare_BeginSession)(
		[ObjectProxyRef = In.GetObjectProxyRef()](FRHICommandListImmediate& RHICmdList)
	{
		ObjectProxyRef->BeginSession_RenderThread();
	});
}

void FTextureShareObjectProxy::EndSession_GameThread(const FTextureShareObject& In)
{
	ENQUEUE_RENDER_COMMAND(TextureShare_EndSession)(
		[ObjectProxyRef = In.GetObjectProxyRef()](FRHICommandListImmediate& RHICmdList)
	{
		ObjectProxyRef->EndSession_RenderThread();
	});
}

void FTextureShareObjectProxy::UpdateProxy_GameThread(const FTextureShareObject& In)
{
	ENQUEUE_RENDER_COMMAND(TextureShare_UpdateObjectProxy)(
		[ObjectProxyRef = In.GetObjectProxyRef()
		, NewTextureShareData = In.TextureShareData
		, SceneViewExtension = In.ViewExtension](FRHICommandListImmediate& RHICmdList)
	{
		ObjectProxyRef->HandleNewFrame_RenderThread(NewTextureShareData, SceneViewExtension);
	});
}
