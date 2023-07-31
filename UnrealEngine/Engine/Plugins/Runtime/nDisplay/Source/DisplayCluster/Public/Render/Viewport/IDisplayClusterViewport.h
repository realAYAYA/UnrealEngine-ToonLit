// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Render/Viewport/IDisplayClusterViewport_CustomPostProcessSettings.h"

#include "Render/Viewport/Containers/DisplayClusterViewport_Context.h"

#include "Render/Viewport/Containers/DisplayClusterViewport_RenderSettings.h"
#include "Render/Viewport/Containers/DisplayClusterViewport_RenderSettingsICVFX.h"
#include "Render/Viewport/Containers/DisplayClusterViewport_PostRenderSettings.h"

#include "Render/Viewport/Containers/DisplayClusterViewport_OverscanSettings.h"

class FSceneViewFamily;

/**
 * Rendering viewport (sub-region of the main viewport)
 */
class DISPLAYCLUSTER_API IDisplayClusterViewport
{
public:
	virtual ~IDisplayClusterViewport() = default;

public:
	virtual FString GetId() const = 0;
	virtual FString GetClusterNodeId() const = 0;

	virtual const FDisplayClusterViewport_RenderSettings&      GetRenderSettings() const = 0;
	virtual const FDisplayClusterViewport_RenderSettingsICVFX& GetRenderSettingsICVFX() const = 0;
	virtual const FDisplayClusterViewport_PostRenderSettings&  GetPostRenderSettings() const = 0;

	// math wrappers: support overscan, etc
	virtual void CalculateProjectionMatrix(const uint32 InContextNum, float Left, float Right, float Top, float Bottom, float ZNear, float ZFar, bool bIsAnglesInput) = 0;
	virtual bool CalculateView(const uint32 InContextNum, FVector& InOutViewLocation, FRotator& InOutViewRotation, const FVector& ViewOffset, const float WorldToMeters, const float NCP, const float FCP) = 0;
	virtual bool GetProjectionMatrix(const uint32 InContextNum, FMatrix& OutPrjMatrix) = 0;

	virtual const TSharedPtr<class IDisplayClusterProjectionPolicy, ESPMode::ThreadSafe>& GetProjectionPolicy() const = 0;

	virtual const TArray<FDisplayClusterViewport_Context>& GetContexts() const = 0;

	// Override postprocess settings for this viewport
	virtual const IDisplayClusterViewport_CustomPostProcessSettings& GetViewport_CustomPostProcessSettings() const = 0;

	// Setup scene view for rendering specified Context
	virtual void SetupSceneView(uint32 ContextNum, class UWorld* World, FSceneViewFamily& InViewFamily, FSceneView& InView) const = 0;

	virtual class IDisplayClusterViewportManager& GetOwner() const = 0;

	virtual void SetRenderSettings(const FDisplayClusterViewport_RenderSettings& InRenderSettings) = 0;
	virtual void SetContexts(TArray<FDisplayClusterViewport_Context>& InContexts) = 0;

	static FMatrix MakeProjectionMatrix(float InLeft, float InRight, float InTop, float InBottom, float ZNear, float ZFar);
};
