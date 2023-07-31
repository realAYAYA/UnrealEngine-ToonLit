// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ITextureShareCallbacks.h"

/**
 * TextureShare callbacks API implementation
 */
class FTextureShareCallbacks :
	public  ITextureShareCallbacks
{
public:
	virtual ~FTextureShareCallbacks() = default;

public:
	virtual FTextureShareBeginSessionEvent& OnTextureShareBeginSession() override
	{
		return TextureShareBeginSessionEvent;
	}
	
	virtual FTextureShareEndSessionEvent& OnTextureShareEndSession() override
	{
		return TextureShareEndSessionEvent;
	}


	virtual FTextureShareBeginFrameSyncEvent& OnTextureShareBeginFrameSync() override
	{
		return TextureShareBeginFrameSyncEvent;
	}

	virtual FTextureShareEndFrameSyncEvent& OnTextureShareEndFrameSync() override
	{
		return TextureShareEndFrameSyncEvent;
	}

	virtual FTextureShareFrameSyncEvent& OnTextureShareFrameSync() override
	{
		return TextureShareFrameSyncEvent;
	}

	virtual FTextureShareBeginFrameSyncEvent_RenderThread& OnTextureShareBeginFrameSyncEvent_RenderThread() override
	{
		return TextureShareBeginFrameSyncEvent_RenderThread;
	}

	virtual FTextureShareEndFrameSyncEvent_RenderThread& OnTextureShareEndFrameSyncEvent_RenderThread() override
	{
		return TextureShareEndFrameSyncEvent_RenderThread;
	}

	virtual FTextureShareFrameSyncEvent_RenderThread& OnTextureShareFrameSyncEvent_RenderThread() override
	{
		return TextureShareFrameSyncEvent_RenderThread;
	}

private:
	FTextureShareBeginSessionEvent     TextureShareBeginSessionEvent;
	FTextureShareEndSessionEvent       TextureShareEndSessionEvent;

	FTextureShareBeginFrameSyncEvent   TextureShareBeginFrameSyncEvent;
	FTextureShareEndFrameSyncEvent     TextureShareEndFrameSyncEvent;
	FTextureShareFrameSyncEvent        TextureShareFrameSyncEvent;

	FTextureShareBeginFrameSyncEvent_RenderThread TextureShareBeginFrameSyncEvent_RenderThread;
	FTextureShareEndFrameSyncEvent_RenderThread   TextureShareEndFrameSyncEvent_RenderThread;
	FTextureShareFrameSyncEvent_RenderThread      TextureShareFrameSyncEvent_RenderThread;
};
