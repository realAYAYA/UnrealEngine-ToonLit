// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Render/Viewport/IDisplayClusterViewport_CustomPostProcessSettings.h"

#include "Render/Viewport/Containers/DisplayClusterViewport_Context.h"
#include "Render/Viewport/Containers/DisplayClusterViewport_RenderSettings.h"
#include "Render/Viewport/Containers/DisplayClusterViewport_RenderSettingsICVFX.h"
#include "Render/Viewport/Containers/DisplayClusterViewport_PostRenderSettings.h"
#include "Render/Viewport/Containers/DisplayClusterViewport_OverscanSettings.h"

#include "Render/Viewport/RenderFrame/DisplayClusterRenderFrameEnums.h"

#include "Engine/EngineTypes.h"

/**
 * nDisplay: Viewport (interface for GameThread)
 */
class DISPLAYCLUSTER_API IDisplayClusterViewport
{
public:
	virtual ~IDisplayClusterViewport() = default;

public:
	/** Get TSharedPtr from self. */
	virtual TSharedPtr<IDisplayClusterViewport, ESPMode::ThreadSafe> ToSharedPtr() = 0;
	virtual TSharedPtr<const IDisplayClusterViewport, ESPMode::ThreadSafe> ToSharedPtr() const = 0;

	virtual FString GetId() const = 0;
	virtual FString GetClusterNodeId() const = 0;

	virtual const FDisplayClusterViewport_RenderSettings&      GetRenderSettings() const = 0;
	virtual const FDisplayClusterViewport_RenderSettingsICVFX& GetRenderSettingsICVFX() const = 0;
	virtual const FDisplayClusterViewport_PostRenderSettings&  GetPostRenderSettings() const = 0;

	// math wrappers: support overscan, etc
	virtual void CalculateProjectionMatrix(const uint32 InContextNum, float Left, float Right, float Top, float Bottom, float ZNear, float ZFar, bool bIsAnglesInput) = 0;
	virtual bool CalculateView(const uint32 InContextNum, FVector& InOutViewLocation, FRotator& InOutViewRotation, const FVector& ViewOffset, const float WorldToMeters, const float NCP, const float FCP) = 0;
	virtual bool GetProjectionMatrix(const uint32 InContextNum, FMatrix& OutPrjMatrix) = 0;

	/** Setup viewpoint for this viewport
	 *
	 * @param InOutViewInfo - [in\out] viewinfo
	 * 
	 * @return - true if there is an internal viewpoint for the given viewport.
	 */
	virtual bool SetupViewPoint(struct FMinimalViewInfo& InOutViewInfo) = 0;

	/** Return view point camera component for this viewport. */
	virtual class UDisplayClusterCameraComponent* GetViewPointCameraComponent() const = 0;

	/** Get the distance from the eye to the viewpoint location.
	 *
	 * @param InContextNum - eye context of this viewport
	 */
	virtual float GetStereoEyeOffsetDistance(const uint32 InContextNum) = 0;

	virtual const TSharedPtr<class IDisplayClusterProjectionPolicy, ESPMode::ThreadSafe>& GetProjectionPolicy() const = 0;

	virtual const TArray<FDisplayClusterViewport_Context>& GetContexts() const = 0;

	// Override postprocess settings for this viewport
	virtual const IDisplayClusterViewport_CustomPostProcessSettings& GetViewport_CustomPostProcessSettings() const = 0;
	virtual IDisplayClusterViewport_CustomPostProcessSettings& GetViewport_CustomPostProcessSettings() = 0;

	// Setup scene view for rendering specified Context
	virtual void SetupSceneView(uint32 ContextNum, class UWorld* World, class FSceneViewFamily& InViewFamily, FSceneView& InView) const = 0;

	/** Return the viewport manager that owns this viewport */
	virtual class IDisplayClusterViewportManager* GetViewportManager() const = 0;

	/** Return the DCRA that owns this viewport */
	virtual class ADisplayClusterRootActor* GetRootActor() const = 0;

	/** Return current render mode. */
	virtual EDisplayClusterRenderFrameMode GetRenderMode() const = 0;

	/** Return current world. */
	virtual class UWorld* GetCurrentWorld() const = 0;

	/** Returns true if the scene is open now (The current world is assigned and DCRA has already initialized for it). */
	virtual bool IsSceneOpened() const = 0;

	/** Returns true if the current world type is equal to one of the input types. */
	virtual bool IsCurrentWorldHasAnyType(const EWorldType::Type InWorldType1, const EWorldType::Type InWorldType2 = EWorldType::None, const EWorldType::Type InWorldType3 = EWorldType::None) const = 0;

	virtual void SetRenderSettings(const FDisplayClusterViewport_RenderSettings& InRenderSettings) = 0;
	virtual void SetContexts(TArray<FDisplayClusterViewport_Context>& InContexts) = 0;

	static FMatrix MakeProjectionMatrix(float InLeft, float InRight, float InTop, float InBottom, float ZNear, float ZFar);
};
