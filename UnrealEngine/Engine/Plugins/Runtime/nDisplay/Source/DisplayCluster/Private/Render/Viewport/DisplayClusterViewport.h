// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Render/Viewport/IDisplayClusterViewport.h"

#include "Render/Viewport/Containers/ImplDisplayClusterViewport_CameraMotionBlur.h"
#include "Render/Viewport/Containers/ImplDisplayClusterViewport_CustomFrustum.h"
#include "Render/Viewport/Containers/ImplDisplayClusterViewport_Overscan.h"
#include "Render/Viewport/Containers/DisplayClusterViewportRemap.h"
#include "Render/Viewport/Containers/DisplayClusterViewport_Enums.h"

#include "Render/Viewport/DisplayClusterViewport_CustomPostProcessSettings.h"
#include "Render/Viewport/DisplayClusterViewport_VisibilitySettings.h"

#include "SceneViewExtensionContext.h"
#include "OpenColorIODisplayExtension.h"

class FDisplayClusterViewportRenderTargetResource;
class FDisplayClusterViewportTextureResource;
class FDisplayClusterViewportManager;
class FDisplayClusterRenderTargetManager;
class FDisplayClusterRenderFrameManager;
class FDisplayClusterViewportProxyData;
class FDisplayClusterViewportProxy;
struct FDisplayClusterRenderFrameSettings;
class FDisplayClusterViewportConfigurationCameraViewport;
class FDisplayClusterViewportConfigurationCameraICVFX;
class FDisplayClusterViewportConfigurationICVFX;

class FDisplayClusterViewportConfigurationHelpers;
class FDisplayClusterViewportConfigurationHelpers_ICVFX;
class FDisplayClusterViewportConfigurationHelpers_OpenColorIO;
class FDisplayClusterViewportConfigurationHelpers_Postprocess;

struct FDisplayClusterViewportConfigurationProjectionPolicy;

/**
 * Rendering viewport (sub-region of the main viewport)
 */

class FDisplayClusterViewport
	: public IDisplayClusterViewport
{
public:
	FDisplayClusterViewport(FDisplayClusterViewportManager& Owner, const FString& ClusterNodeId, const FString& ViewportId, const TSharedPtr<IDisplayClusterProjectionPolicy, ESPMode::ThreadSafe>& InProjectionPolicy);
	
	virtual ~FDisplayClusterViewport();

public:
	//////////////////////////////////////////////////////
	/// IDisplayClusterViewport
	//////////////////////////////////////////////////////
	virtual FString GetId() const override
	{ 
		check(IsInGameThread());
		return ViewportId; 
	}

	virtual FString GetClusterNodeId() const override
	{
		check(IsInGameThread());
		return ClusterNodeId;
	}

	virtual const FDisplayClusterViewport_RenderSettings& GetRenderSettings() const override
	{
		check(IsInGameThread());
		return RenderSettings;
	}

	virtual void SetRenderSettings(const FDisplayClusterViewport_RenderSettings& InRenderSettings) override
	{
		check(IsInGameThread());
		RenderSettings = InRenderSettings;
	}

	virtual void SetContexts(TArray<FDisplayClusterViewport_Context>& InContexts) override
	{
		check(IsInGameThread());
		Contexts.Empty();
		Contexts.Append(InContexts);
	}

	virtual void CalculateProjectionMatrix(const uint32 InContextNum, float Left, float Right, float Top, float Bottom, float ZNear, float ZFar, bool bIsAnglesInput) override;

	virtual bool    CalculateView(const uint32 InContextNum, FVector& InOutViewLocation, FRotator& InOutViewRotation, const FVector& ViewOffset, const float WorldToMeters, const float NCP, const float FCP) override;
	virtual bool    GetProjectionMatrix(const uint32 InContextNum, FMatrix& OutPrjMatrix)  override;

	virtual const FDisplayClusterViewport_RenderSettingsICVFX& GetRenderSettingsICVFX() const override
	{
		check(IsInGameThread());
		return RenderSettingsICVFX;
	}

	virtual const FDisplayClusterViewport_PostRenderSettings& GetPostRenderSettings() const override
	{
		check(IsInGameThread());
		return PostRenderSettings;
	}

	virtual const TSharedPtr<IDisplayClusterProjectionPolicy, ESPMode::ThreadSafe>& GetProjectionPolicy() const override
	{
		check(IsInGameThread());
		return ProjectionPolicy;
	}

	virtual const TArray<FDisplayClusterViewport_Context>& GetContexts() const override
	{
		check(IsInGameThread());
		return Contexts;
	}

	virtual const IDisplayClusterViewport_CustomPostProcessSettings& GetViewport_CustomPostProcessSettings() const override
	{
		check(IsInGameThread());
		return CustomPostProcessSettings;
	}

	// Setup scene view for rendering specified Context
	virtual void SetupSceneView(uint32 ContextNum, class UWorld* World, FSceneViewFamily& InViewFamily, FSceneView& InView) const override;

	virtual IDisplayClusterViewportManager& GetOwner() const override;

	FDisplayClusterViewportManager& ImplGetOwner() const
	{
		return Owner;
	}

	const FDisplayClusterRenderFrameSettings& GetRenderFrameSettings() const;

	//////////////////////////////////////////////////////
	/// ~IDisplayClusterViewport
	//////////////////////////////////////////////////////

#if WITH_EDITOR
	FSceneView* ImplCalcScenePreview(class FSceneViewFamilyContext& InOutViewFamily, uint32 ContextNum);
	bool    ImplPreview_CalculateStereoViewOffset(const uint32 InContextNum, FRotator& ViewRotation, const float WorldToMeters, FVector& ViewLocation);
	FMatrix ImplPreview_GetStereoProjectionMatrix(const uint32 InContextNum);

	bool GetPreviewPixels(TSharedPtr<class FDisplayClusterViewportReadPixelsData, ESPMode::ThreadSafe>& OutPixelsData) const;

#endif //WITH_EDITOR

	// Get from logic request for additional targetable resource
	bool ShouldUseAdditionalTargetableResource() const;
	bool ShouldUseAdditionalFrameTargetableResource() const;
	bool ShouldUseFullSizeFrameTargetableResource() const;

	inline bool FindContext(const int32 ViewIndex, uint32* OutContextNum)
	{
		check(IsInGameThread());

		for (int32 ContextNum = 0; ContextNum < Contexts.Num(); ContextNum++)
		{
			if (ViewIndex == Contexts[ContextNum].StereoViewIndex)
			{
				if (OutContextNum != nullptr)
				{
					*OutContextNum = ContextNum;
				}

				return true;
			}
		}

		return false;
	}

	bool HandleStartScene();
	void HandleEndScene();

	void AddReferencedObjects(FReferenceCollector& Collector);

	void ResetRuntimeParameters()
	{
		// Reset runtim flags from prev frame:
		RenderSettings.BeginUpdateSettings();
		RenderSettingsICVFX.BeginUpdateSettings();
		PostRenderSettings.BeginUpdateSettings();
		VisibilitySettings.ResetConfiguration();
		CameraMotionBlur.ResetConfiguration();
		OverscanRendering.ResetConfiguration();
		CustomFrustumRendering.ResetConfiguration();
	}

	// Active view extension for this viewport
	const TArray<FSceneViewExtensionRef> GatherActiveExtensions(FViewport* InViewport) const;

	bool UpdateFrameContexts(const uint32 InStereoViewIndex, const FDisplayClusterRenderFrameSettings& InFrameSettings);

public:
	FIntRect GetValidRect(const FIntRect& InRect, const TCHAR* DbgSourceName);

private:
	float GetClusterRenderTargetRatioMult(const FDisplayClusterRenderFrameSettings& InFrameSettings) const;
	FIntPoint GetDesiredContextSize(const FIntPoint& InSize, const FDisplayClusterRenderFrameSettings& InFrameSettings) const;
	float GetCustomBufferRatio(const FDisplayClusterRenderFrameSettings& InFrameSettings) const;

#if WITH_EDITOR
	// Support view states for preview
private:
	FSceneViewStateInterface* GetViewState(uint32 ViewIndex);
	void CleanupViewState();

private:
	TArray<FSceneViewStateReference> ViewStates;
#endif

public:
	// Support OCIO:
	FSceneViewExtensionIsActiveFunctor GetSceneViewExtensionIsActiveFunctor() const;

	// OCIO wrapper
	TSharedPtr<FOpenColorIODisplayExtension, ESPMode::ThreadSafe> OpenColorIODisplayExtension;

public:
	// Projection policy instance that serves this viewport
	TSharedPtr<IDisplayClusterProjectionPolicy, ESPMode::ThreadSafe> ProjectionPolicy;
	TSharedPtr<IDisplayClusterProjectionPolicy, ESPMode::ThreadSafe> UninitializedProjectionPolicy;

	// Game thread only settings:
	FDisplayClusterViewport_CustomPostProcessSettings CustomPostProcessSettings;
	FDisplayClusterViewport_VisibilitySettings        VisibilitySettings;

	// Additional features:
	FImplDisplayClusterViewport_CameraMotionBlur CameraMotionBlur;
	FImplDisplayClusterViewport_Overscan         OverscanRendering;
	FImplDisplayClusterViewport_CustomFrustum    CustomFrustumRendering;

	// viewport OutputRemap feature
	FDisplayClusterViewportRemap ViewportRemap;

protected:
	friend FDisplayClusterViewportProxy;
	friend FDisplayClusterViewportProxyData;
	friend FDisplayClusterViewportManager;
	friend FDisplayClusterRenderTargetManager;
	friend FDisplayClusterRenderFrameManager;
	friend FDisplayClusterViewportConfigurationCameraViewport;
	friend FDisplayClusterViewportConfigurationCameraICVFX;

	friend FDisplayClusterViewportConfigurationICVFX;

	friend FDisplayClusterViewportConfigurationHelpers;
	friend FDisplayClusterViewportConfigurationHelpers_ICVFX;
	friend FDisplayClusterViewportConfigurationHelpers_OpenColorIO;
	friend FDisplayClusterViewportConfigurationHelpers_Postprocess;
	friend FDisplayClusterViewportConfigurationProjectionPolicy;

	friend FDisplayClusterViewportRemap;

	// viewport render thread data
	TSharedPtr<FDisplayClusterViewportProxy, ESPMode::ThreadSafe> ViewportProxy;

	// Unique viewport name
	const FString ViewportId;

	// Owner cluster node name
	const FString ClusterNodeId;

	// Viewport render params
	FDisplayClusterViewport_RenderSettings       RenderSettings;
	FDisplayClusterViewport_RenderSettingsICVFX  RenderSettingsICVFX;
	FDisplayClusterViewport_PostRenderSettings   PostRenderSettings;

	// Viewport contexts (left/center/right eyes)
	TArray<FDisplayClusterViewport_Context> Contexts;

	// View family render to this resources
	TArray<FDisplayClusterViewportRenderTargetResource*> RenderTargets;
	
	// Projection policy output resources
	TArray<FDisplayClusterViewportTextureResource*> OutputFrameTargetableResources;
	TArray<FDisplayClusterViewportTextureResource*> AdditionalFrameTargetableResources;

#if WITH_EDITOR
	friend class UDisplayClusterPreviewComponent;

	FTextureRHIRef OutputPreviewTargetableResource;
#endif

	// unique viewport resources
	TArray<FDisplayClusterViewportTextureResource*> InputShaderResources;
	TArray<FDisplayClusterViewportTextureResource*> AdditionalTargetableResources;
	TArray<FDisplayClusterViewportTextureResource*> MipsShaderResources;

	FDisplayClusterViewportManager& Owner;

private:
	bool bProjectionPolicyCalculateViewWarningOnce = false;
};
