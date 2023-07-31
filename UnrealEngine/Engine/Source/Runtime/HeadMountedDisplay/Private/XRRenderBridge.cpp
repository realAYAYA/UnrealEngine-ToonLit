// Copyright Epic Games, Inc. All Rights Reserved.

#include "XRRenderBridge.h"

void FXRRenderBridge::UpdateViewport(const class FViewport& Viewport, class FRHIViewport* InViewportRHI)
{
	check(IsInGameThread());
	check(InViewportRHI);
}
