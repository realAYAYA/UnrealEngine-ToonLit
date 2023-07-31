// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RHI.h"
#include "RHIResources.h"

class HEADMOUNTEDDISPLAY_API  FXRRenderBridge : public FRHICustomPresent
{
public:

	FXRRenderBridge() {}

	// virtual methods from FRHICustomPresent

	virtual void OnBackBufferResize() override {}

	virtual bool NeedsNativePresent() override
	{
		return true;
	}

	// Overridable by subclasses

	/** 
	 * Override this method in case the render bridge needs access to the current viewport or RHI viewport before
	 * rendering the current frame. Note that you *should not* call Viewport->SetCustomPresent() from this method, as
	 * that is handled by the XRRenderTargetManager implementation.
	 */
	virtual void UpdateViewport(const class FViewport& Viewport, class FRHIViewport* InViewportRHI);
};