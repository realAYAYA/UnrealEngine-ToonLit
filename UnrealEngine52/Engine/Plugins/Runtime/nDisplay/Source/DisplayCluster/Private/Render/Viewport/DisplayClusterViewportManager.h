// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "SceneTypes.h"

#include "Misc/DisplayClusterObjectRef.h"

#include "Render/Viewport/IDisplayClusterViewportManager.h"
#include "Render/Viewport/DisplayClusterViewport.h"

#include "Render/Viewport/Containers/DisplayClusterViewport_Enums.h"

class FDisplayClusterViewportConfiguration;
class FDisplayClusterRenderTargetManager;
class FDisplayClusterViewportPostProcessManager;
class FDisplayClusterRenderFrameManager;
class FDisplayClusterRenderFrame; 
class FDisplayClusterViewportManagerProxy;
class FDisplayClusterViewportLightCardManager;
class IDisplayClusterProjectionPolicy;

class  UDisplayClusterConfigurationViewport;
struct FDisplayClusterConfigurationProjection;
struct FDisplayClusterRenderFrameSettings;

class FViewport;

class FDisplayClusterViewportManager
	: public IDisplayClusterViewportManager
{
public:
	FDisplayClusterViewportManager();
	virtual ~FDisplayClusterViewportManager();

public:
	virtual const IDisplayClusterViewportManagerProxy* GetProxy() const override;
	virtual       IDisplayClusterViewportManagerProxy* GetProxy() override;

	virtual UWorld*                   GetCurrentWorld() const override;
	virtual ADisplayClusterRootActor* GetRootActor() const override;

	/** Game thread funcs */
	void StartScene(UWorld* World);
	void EndScene();
	void ResetScene();

	virtual bool IsSceneOpened() const override;

	virtual bool UpdateConfiguration(EDisplayClusterRenderFrameMode InRenderMode, const FString& InClusterNodeId, class ADisplayClusterRootActor* InRootActorPtr, const FDisplayClusterPreviewSettings* InPreviewSettings = nullptr) override;
	virtual bool UpdateCustomConfiguration(EDisplayClusterRenderFrameMode InRenderMode, const TArray<FString>& InViewportNames, class ADisplayClusterRootActor* InRootActorPtr) override;

	virtual bool BeginNewFrame(FViewport* InViewport, UWorld* InWorld, FDisplayClusterRenderFrame& OutRenderFrame) override;

	virtual void InitializeNewFrame() override;
	virtual void FinalizeNewFrame() override;

	virtual FSceneViewFamily::ConstructionValues CreateViewFamilyConstructionValues(
		const FDisplayClusterRenderFrame::FFrameRenderTarget& InFrameTarget,
		FSceneInterface* InScene,
		FEngineShowFlags InEngineShowFlags,
		const bool bInAdditionalViewFamily
	) const override;

	virtual void ConfigureViewFamily(const FDisplayClusterRenderFrame::FFrameRenderTarget& InFrameTarget, const FDisplayClusterRenderFrame::FFrameViewFamily& InFrameViewFamily, FSceneViewFamilyContext& InOutViewFamily) override;
	
	virtual void RenderFrame(FViewport* InViewport) override;

#if WITH_EDITOR
	virtual bool RenderInEditor(class FDisplayClusterRenderFrame& InRenderFrame, FViewport* InViewport, const uint32 InFirstViewportNum, const int32 InViewportsAmount, int32& OutViewportsAmount, bool& bOutFrameRendered) override;
	
	void ImplUpdatePreviewRTTResources();

	const TArray<FDisplayClusterViewport*>& ImplGetWholeClusterViewports_Editor() const
	{
		check(IsInGameThread());

		return Viewports;
	}

	// Current world is a tools window (configurator, other tools)
	bool IsEditorPreviewWorld() const;

private:
	/** Called before garbage collection is run */
	void OnPreGarbageCollect();
#endif
public:

	virtual IDisplayClusterViewport* FindViewport(const FString& InViewportId) const override
	{
		return ImplFindViewport(InViewportId);
	}

	virtual IDisplayClusterViewport* FindViewport(const int32 ViewIndex, uint32* OutContextNum = nullptr) const override;

	virtual const TArrayView<IDisplayClusterViewport*> GetViewports() const override
	{
		return TArrayView<IDisplayClusterViewport*>((IDisplayClusterViewport**)(ClusterNodeViewports.GetData()), ClusterNodeViewports.Num());
	}

	virtual void MarkComponentGeometryDirty(const FName InComponentName = NAME_None) override;

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;

	// internal use only
	bool CreateViewport(const FString& ViewportId, const class UDisplayClusterConfigurationViewport* ConfigurationViewport);
	IDisplayClusterViewport* CreateViewport(const FString& ViewportId, const TSharedPtr<IDisplayClusterProjectionPolicy, ESPMode::ThreadSafe>& InProjectionPolicy);
	bool                     DeleteViewport(const FString& ViewportId);

	FDisplayClusterViewport* ImplCreateViewport(const FString& ViewportId, const TSharedPtr<IDisplayClusterProjectionPolicy, ESPMode::ThreadSafe>& InProjectionPolicy);
	void ImplDeleteViewport(FDisplayClusterViewport* Viewport);

	const TArray<FDisplayClusterViewport*>& ImplGetViewports() const
	{
		check(IsInGameThread());
		return ClusterNodeViewports;
	}

	FDisplayClusterViewport* ImplFindViewport(const FString& InViewportId) const;

	static TSharedPtr<IDisplayClusterProjectionPolicy, ESPMode::ThreadSafe> CreateProjectionPolicy(const FString& InViewportId, const FDisplayClusterConfigurationProjection* InConfigurationProjectionPolicy);

	TSharedPtr<FDisplayClusterViewportManagerProxy, ESPMode::ThreadSafe> GetViewportManagerProxy() const
	{
		check(IsInGameThread());

		return ViewportManagerProxy;
	}

	const FDisplayClusterRenderFrameSettings& GetRenderFrameSettings() const;

	/** Return number of contexts per viewport. */
	int32 GetViewPerViewportAmount() const;

	/** Return initial StereoViewIndex for the input viewport. */
	int32 FindFirstViewportStereoViewIndex(FDisplayClusterViewport* InViewport) const;

	/** Returns true if the final color is renderable (to support preview in scene). */
	bool ShouldRenderFinalColor() const;

	/** Return nDisplay VE object. */
	TSharedPtr<class FDisplayClusterViewportManagerViewExtension, ESPMode::ThreadSafe> GetViewportManagerViewExtension() const
	{ return ViewportManagerViewExtension; }

	/** Return LightCardManager object. */
	TSharedPtr<FDisplayClusterViewportLightCardManager, ESPMode::ThreadSafe> GetLightCardManager() const
	{ return LightCardManager; }

	/** Return PostProcessManager object. */
	TSharedPtr<FDisplayClusterViewportPostProcessManager, ESPMode::ThreadSafe> GetPostProcessManager() const
	{ return PostProcessManager; }

	bool ShouldUseAdditionalFrameTargetableResource() const;
	bool ShouldUseFullSizeFrameTargetableResource() const;

	void SetViewportBufferRatio(FDisplayClusterViewport& DstViewport, float InBufferRatio);
	void ResetSceneRenderTargetSize();

private:
	void UpdateDesiredNumberOfViews(FDisplayClusterRenderFrame& InOutRenderFrame);
	void UpdateSceneRenderTargetSize();
	void HandleViewportRTTChanges(const TArray<FDisplayClusterViewport_Context>& PrevContexts, const TArray<FDisplayClusterViewport_Context>& Contexts);
	void ImplUpdateClusterNodeViewports(const EDisplayClusterRenderFrameMode InRenderMode, const FString& InClusterNodeId);

	/** Register any callbacks */
	void RegisterCallbacks();
	
	/** Unregister any callbacks used */
	void UnregisterCallbacks();
	
protected:
	friend FDisplayClusterViewportManagerProxy;
	friend FDisplayClusterViewportConfiguration;

	TSharedPtr<FDisplayClusterRenderTargetManager, ESPMode::ThreadSafe>        RenderTargetManager;
	TSharedPtr<FDisplayClusterViewportPostProcessManager, ESPMode::ThreadSafe> PostProcessManager;
	TSharedPtr<FDisplayClusterViewportLightCardManager, ESPMode::ThreadSafe>   LightCardManager;

public:
	TUniquePtr<FDisplayClusterViewportConfiguration> Configuration;

private:
	TUniquePtr<FDisplayClusterRenderFrameManager>  RenderFrameManager;

	TArray<FDisplayClusterViewport*> Viewports;
	TArray<FDisplayClusterViewport*> ClusterNodeViewports;

	/** Render thread proxy manager. Deleted on render thread */
	TSharedPtr<FDisplayClusterViewportManagerProxy, ESPMode::ThreadSafe> ViewportManagerProxy;

	// Pointer to the current scene
	TWeakObjectPtr<UWorld> CurrentWorldRef;

	enum class ESceneRenderTargetResizeMethod : uint8
	{
		None = 0,
		Reset,
		WaitFrameSizeHistory,
		Restore
	};

	// Support for resetting RTT size (GROW method always grows and does not recover FPS when the viewport size or buffer ratio is changed)
	ESceneRenderTargetResizeMethod SceneRenderTargetResizeMethod = ESceneRenderTargetResizeMethod::None;
	int32 FrameHistoryCounter = 0;

	// Handle special features
	TSharedPtr<class FDisplayClusterViewportManagerViewExtension, ESPMode::ThreadSafe> ViewportManagerViewExtension;

#if WITH_EDITOR
	/** The handle for FCoreUObjectDelegates::GetPreGarbageCollectDelegate */
	FDelegateHandle PreGarbageCollectHandle;
#endif
};
