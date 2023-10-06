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
#include "Render/Viewport/DisplayClusterViewport_OpenColorIO.h"

#include "SceneViewExtension.h"
#include "SceneViewExtensionContext.h"

#include "Templates/SharedPointer.h"

class FDisplayClusterViewportRenderTargetResource;
class FDisplayClusterViewportTextureResource;
class FDisplayClusterViewportManager;
class FDisplayClusterViewportManagerProxy;
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
	, public TSharedFromThis<FDisplayClusterViewport, ESPMode::ThreadSafe>
{
public:
	FDisplayClusterViewport(FDisplayClusterViewportManager& Owner, const FString& ClusterNodeId, const FString& ViewportId, const TSharedPtr<IDisplayClusterProjectionPolicy, ESPMode::ThreadSafe>& InProjectionPolicy);
	
	virtual ~FDisplayClusterViewport();

public:
	//////////////////////////////////////////////////////
	/// IDisplayClusterViewport
	//////////////////////////////////////////////////////
	virtual TSharedPtr<IDisplayClusterViewport, ESPMode::ThreadSafe> ToSharedPtr() override
	{
		return AsShared();
	}

	virtual TSharedPtr<const IDisplayClusterViewport, ESPMode::ThreadSafe> ToSharedPtr() const override
	{
		return AsShared();
	}

	virtual EDisplayClusterRenderFrameMode GetRenderMode() const override;

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

	virtual bool SetupViewPoint(FMinimalViewInfo& InOutViewInfo) override;
	virtual float GetStereoEyeOffsetDistance(const uint32 InContextNum) override;
	virtual class UDisplayClusterCameraComponent* GetViewPointCameraComponent() const override;

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

	virtual IDisplayClusterViewport_CustomPostProcessSettings& GetViewport_CustomPostProcessSettings() override
	{
		check(IsInGameThread());
		return CustomPostProcessSettings;
	}

	// Setup scene view for rendering specified Context
	virtual void SetupSceneView(uint32 ContextNum, class UWorld* World, class FSceneViewFamily& InViewFamily, FSceneView& InView) const override;

	virtual class IDisplayClusterViewportManager* GetViewportManager() const override;
	virtual class ADisplayClusterRootActor* GetRootActor() const override;
	virtual class UWorld* GetCurrentWorld() const override;
	virtual bool IsSceneOpened() const override;

	// Return true, if current world type equal to InWorldType
	virtual bool IsCurrentWorldHasAnyType(const EWorldType::Type InWorldType1, const EWorldType::Type InWorldType2 = EWorldType::None, const EWorldType::Type InWorldType3 = EWorldType::None) const override;

	//////////////////////////////////////////////////////
	/// ~IDisplayClusterViewport
	//////////////////////////////////////////////////////

	TSharedPtr<FDisplayClusterViewportManager, ESPMode::ThreadSafe> GetViewportManagerRefImpl() const;
	TSharedPtr<FDisplayClusterViewportManagerProxy, ESPMode::ThreadSafe> GetViewportManagerProxyRefImpl() const;

	FDisplayClusterViewportManager* GetViewportManagerImpl() const;
	FDisplayClusterViewportManagerProxy* GetViewportManagerProxyImpl() const;

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

	void SetViewportBufferRatio(const float InBufferRatio);

	const FDisplayClusterRenderFrameSettings* GetRenderFrameSettings() const;

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

	/** Initialize viewport contexts and resources for new frame
	 *
	 * @param InStereoViewIndex - initial StereoViewIndex for this viewport
	 * @param InFrameSettings   - New settings for current frame
	 *
	 * @return - true, if success
	 */
	bool UpdateFrameContexts(const uint32 InStereoViewIndex, const FDisplayClusterRenderFrameSettings& InFrameSettings);

	/** Reset viewport contexts and resources. */
	void ResetFrameContexts();

	/** Compare OCIO with another viewport, return true if they are equal. */
	bool IsOpenColorIOEquals(const FDisplayClusterViewport& InViewport) const;

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

public:
	void CleanupViewState();

private:
	TArray<TSharedPtr<FSceneViewStateReference, ESPMode::ThreadSafe>> ViewStates;
#endif

public:
	/** nDisplay OpenColorIO object. */
	TSharedPtr<FDisplayClusterViewport_OpenColorIO, ESPMode::ThreadSafe> OpenColorIO;

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

	// viewport owners
	TWeakPtr<FDisplayClusterViewportManager, ESPMode::ThreadSafe> ViewportManagerWeakPtr;
	TWeakPtr<FDisplayClusterViewportManagerProxy, ESPMode::ThreadSafe> ViewportManagerProxyWeakPtr;

private:
	bool bProjectionPolicyCalculateViewWarningOnce = false;
};
