// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Containers/TextureShareContainers.h"
#include "Containers/TextureShareCoreEnums.h"

class ITextureShareObject;
class ITextureShareObjectProxy;
class FRHICommandListImmediate;

/**
 * TextureShare callbacks API
 */
class ITextureShareCallbacks
{
public:
	static ITextureShareCallbacks& Get();

	virtual ~ITextureShareCallbacks() = default;

public:
	/** Called on session start **/
	DECLARE_EVENT_OneParam(ITextureShareCallbacks, FTextureShareBeginSessionEvent, ITextureShareObject&);
	virtual FTextureShareBeginSessionEvent& OnTextureShareBeginSession() = 0;

	/** Called on session end **/
	DECLARE_EVENT_OneParam(ITextureShareCallbacks, FTextureShareEndSessionEvent, ITextureShareObject&);
	virtual FTextureShareEndSessionEvent& OnTextureShareEndSession() = 0;

	/** Called on begin frame sync **/
	DECLARE_EVENT_OneParam(ITextureShareCallbacks, FTextureShareBeginFrameSyncEvent, ITextureShareObject&);
	virtual FTextureShareBeginFrameSyncEvent& OnTextureShareBeginFrameSync() = 0;

	/** Called on end frame sync **/
	DECLARE_EVENT_OneParam(ITextureShareCallbacks, FTextureShareEndFrameSyncEvent, ITextureShareObject&);
	virtual FTextureShareEndFrameSyncEvent& OnTextureShareEndFrameSync() = 0;

	/** Called on frame sync **/
	DECLARE_EVENT_TwoParams(ITextureShareCallbacks, FTextureShareFrameSyncEvent, ITextureShareObject&, const ETextureShareSyncStep);
	virtual FTextureShareFrameSyncEvent& OnTextureShareFrameSync() = 0;


	/** Called on begin frame sync on render thread **/
	DECLARE_EVENT_TwoParams(ITextureShareCallbacks, FTextureShareBeginFrameSyncEvent_RenderThread, FRHICommandListImmediate&, const ITextureShareObjectProxy&);
	virtual FTextureShareBeginFrameSyncEvent_RenderThread& OnTextureShareBeginFrameSyncEvent_RenderThread() = 0;

	/** Called on end frame sync on render thread **/
	DECLARE_EVENT_TwoParams(ITextureShareCallbacks, FTextureShareEndFrameSyncEvent_RenderThread, FRHICommandListImmediate&, const ITextureShareObjectProxy&);
	virtual FTextureShareEndFrameSyncEvent_RenderThread& OnTextureShareEndFrameSyncEvent_RenderThread() = 0;

	/** Called on frame sync on render thread **/
	DECLARE_EVENT_ThreeParams(ITextureShareCallbacks, FTextureShareFrameSyncEvent_RenderThread, FRHICommandListImmediate&, const ITextureShareObjectProxy&, const ETextureShareSyncStep);
	virtual FTextureShareFrameSyncEvent_RenderThread& OnTextureShareFrameSyncEvent_RenderThread() = 0;
};
