// Copyright Epic Games, Inc. All Rights Reserved.

#include "TextureShareObject.h"
#include "Misc/TextureShareLog.h"

//////////////////////////////////////////////////////////////////////////////////////////////
// FTextureShareObject Sync
//////////////////////////////////////////////////////////////////////////////////////////////
bool FTextureShareObject::IsFrameSyncActive() const
{
	return TextureShareSDKObject->IsFrameSyncActive();
}

bool FTextureShareObject::IsFrameSyncActive_RenderThread() const
{
	return TextureShareSDKObject->IsFrameSyncActive_RenderThread();
}

bool FTextureShareObject::BeginFrame()
{
	// Game thread frame sync
	if (TextureShareSDKObject->IsActive())
	{
		// Update sync settings before new frame
		UpdateSyncSettings();

		// Handle frame (game thread)
		if (TextureShareSDKObject->IsBeginFrameSyncActive()
			&& TextureShareSDKObject->BeginFrameSync())
		{
			// Set game thread data
			TextureShareSDKObject->SetData(TDataInput<FTextureShareCoreData>(Data));

			// Finish game thread frame
			if (TextureShareSDKObject->EndFrameSync())
			{
				// And immediatelly enter to proxy frame (rendering thread)
				if (TextureShareSDKObject->IsBeginFrameSyncActive_RenderThread()
					&& TextureShareSDKObject->BeginFrameSync_RenderThread())
				{
					CurrentFrameProxySyncStep = ETextureShareSyncStep::FrameProxyBegin;
					DefferedProxySyncStep = ETextureShareSyncStep::Undefined;

					return true;
				}

				DefferedProxySyncStep = CurrentFrameProxySyncStep = ETextureShareSyncStep::Undefined;
			}
		}
	}

	return false;
}

bool FTextureShareObject::EndFrame()
{
	if (TextureShareSDKObject->IsFrameSyncActive_RenderThread())
	{
		return TextureShareSDKObject->EndFrameSync_RenderThread();
	}

	return false;
}

bool FTextureShareObject::FrameSync(const ETextureShareSyncStep InSyncStep)
{
	if (InSyncStep == ETextureShareSyncStep::Undefined)
	{
		return true;
	}

	if (TextureShareSDKObject->FrameSync(InSyncStep))
	{
		// Update received data
		ReceivedCoreObjectData.Empty();
		TextureShareSDKObject->GetReceivedData(*TDataOutput<TArraySerializable<FTextureShareCoreObjectData>>(ReceivedCoreObjectData));

		return true;
	}

	return false;
}

bool FTextureShareObject::ResourceSync_RenderThread(const FTextureShareCoreResourceDesc& InResourceDesc)
{
	const ETextureShareSyncStep InSyncStep = InResourceDesc.SyncStep;
	if (InSyncStep == ETextureShareSyncStep::Undefined)
	{
		return true;
	}

	if (DefferedProxySyncStep != ETextureShareSyncStep::Undefined)
	{
		if (DefferedProxySyncStep < InSyncStep)
		{
			const ETextureShareSyncStep InDefferedSyncStep = DefferedProxySyncStep;
			DefferedProxySyncStep = ETextureShareSyncStep::Undefined;

			if (!FrameSync_RenderThread(InDefferedSyncStep))
			{
				return false;
			}
		}
	}

	switch (InResourceDesc.OperationType)
	{
	case ETextureShareTextureOp::Read:
		// Before reading texture sync must be done first
		return (CurrentFrameProxySyncStep == InSyncStep) ? true : FrameSync_RenderThread(InSyncStep);

	case ETextureShareTextureOp::Write:
		// Syncing output textures must be called at end
		DefferedProxySyncStep = InSyncStep;
		return true;

	default:
		break;
	}

	return false;
}

bool FTextureShareObject::FrameSync_RenderThread(const ETextureShareSyncStep InSyncStep)
{
	if (InSyncStep == ETextureShareSyncStep::Undefined)
	{
		return true;
	}

	if (CurrentFrameProxySyncStep < InSyncStep && InSyncStep > ETextureShareSyncStep::FrameProxyBegin && InSyncStep < ETextureShareSyncStep::FrameProxyEnd)
	{
		CurrentFrameProxySyncStep = InSyncStep;

		if (TextureShareSDKObject->FrameSync_RenderThread(InSyncStep))
		{
			// Update received data
			ReceivedCoreObjectProxyData.Empty();
			TextureShareSDKObject->GetReceivedProxyData_RenderThread(*TDataOutput<TArraySerializable<FTextureShareCoreObjectProxyData>>(ReceivedCoreObjectProxyData));

			return true;
		}
	}

	return false;
}
