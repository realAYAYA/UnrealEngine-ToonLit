// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetalRHIVisionOSBridge.h"

#if PLATFORM_VISIONOS

#include "MetalRHIPrivate.h"
#include "MetalDynamicRHI.h"

DEFINE_LOG_CATEGORY(LogMetalVisionOS);

void MetalRHIVisionOS::PresentImmersive(const MetalRHIVisionOS::PresentImmersiveParams& Params)
{
    FMetalRHICommandContext* RHICommandContext = static_cast<FMetalRHICommandContext*>(RHIGetDefaultContext());
    check(RHICommandContext);
    check(RHICommandContext->CustomPresentViewport);
    FMetalViewport* Viewport = ResourceCast(RHICommandContext->CustomPresentViewport);
    check(Viewport);

    Viewport->PresentImmersive(Params);
}
#endif // PLATFORM_VISIONOS
