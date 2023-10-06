// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "SceneTypes.h"
#include "Templates/SharedPointer.h"

#include "Misc/DisplayClusterObjectRef.h"

#include "Render/Viewport/IDisplayClusterViewportManager.h"
#include "Render/Viewport/DisplayClusterViewport.h"
#include "Render/Viewport/Containers/DisplayClusterViewport_Enums.h"

class FDisplayClusterViewportConfiguration;
class FDisplayClusterRenderTargetManager;
class FDisplayClusterViewportPostProcessManager;
class FDisplayClusterRenderFrameManager;
class FDisplayClusterViewportManagerProxy;
class FDisplayClusterViewportLightCardManager;
class IDisplayClusterProjectionPolicy;

class  UDisplayClusterConfigurationViewport;
struct FDisplayClusterConfigurationProjection;
struct FDisplayClusterRenderFrameSettings;

class FViewport;

class FDisplayClusterViewportManager
	: public IDisplayClusterViewportManager
	, public TSharedFromThis<FDisplayClusterViewportManager, ESPMode::ThreadSafe>
{
public:
	FDisplayClusterViewportManager();
	virtual ~FDisplayClusterViewportManager();

public:
	/** Game thread funcs */
	void StartScene(UWorld* World);
	void EndScene();
	void ResetScene();

	void Initialize();

	//~ Begin IDisplayClusterViewportManager
	virtual TSharedPtr<IDisplayClusterViewportManager, ESPMode::ThreadSafe> ToSharedPtr() override
	{
		return AsShared();
	}

	virtual TSharedPtr<const IDisplayClusterViewportManager, ESPMode::ThreadSafe> ToSharedPtr() const override
	{
		return AsShared();
	}

	virtual EDisplayClusterRenderFrameMode GetRenderMode() const override;

	virtual const IDisplayClusterViewportManagerProxy* GetProxy() const override;
	virtual       IDisplayClusterViewportManagerProxy* GetProxy() override;

	virtual UWorld*                   GetCurrentWorld() const override;
	virtual ADisplayClusterRootActor* GetRootActor() const override;

	virtual bool IsSceneOpened() const override;

	virtual bool UpdateConfiguration(EDisplayClusterRenderFrameMode InRenderMode, const FString& InClusterNodeId, class ADisplayClusterRootActor* InRootActorPtr, const FDisplayClusterPreviewSettings* InPreviewSettings = nullptr) override;
	virtual bool UpdateCustomConfiguration(EDisplayClusterRenderFrameMode InRenderMode, const TArray<FString>& InViewportNames, class ADisplayClusterRootActor* InRootActorPtr) override;

	virtual bool BeginNewFrame(FViewport* InViewport, UWorld* InWorld, FDisplayClusterRenderFrame& OutRenderFrame) override;
	virtual void InitializeNewFrame() override;
	virtual void FinalizeNewFrame() override;

	virtual FSceneViewFamily::ConstructionValues CreateViewFamilyConstructionValues(
		const FDisplayClusterRenderFrameTarget& InFrameTarget,
		FSceneInterface* InScene,
		FEngineShowFlags InEngineShowFlags,
		const bool bInAdditionalViewFamily
	) const override;

	virtual void ConfigureViewFamily(const FDisplayClusterRenderFrameTarget& InFrameTarget, const FDisplayClusterRenderFrameTargetViewFamily& InFrameViewFamily, FSceneViewFamilyContext& InOutViewFamily) override;
	
	virtual void RenderFrame(FViewport* InViewport) override;

#if WITH_EDITOR
	virtual bool RenderInEditor(FDisplayClusterRenderFrame& InRenderFrame, FViewport* InViewport, const uint32 InFirstViewportNum, const int32 InViewportsAmount, int32& OutViewportsAmount, bool& bOutFrameRendered) override;
	
	void ImplUpdatePreviewRTTResources();

private:
	/** Called before garbage collection is run */
	void OnPreGarbageCollect();

#endif
public:

	virtual IDisplayClusterViewport* FindViewport(const FString& InViewportId) const override;
	virtual IDisplayClusterViewport* FindViewport(const int32 ViewIndex, uint32* OutContextNum = nullptr) const override;

	virtual const TArrayView<TSharedPtr<IDisplayClusterViewport, ESPMode::ThreadSafe>> GetViewports() const override
	{
		return TArrayView<TSharedPtr<IDisplayClusterViewport, ESPMode::ThreadSafe>>((TSharedPtr<IDisplayClusterViewport, ESPMode::ThreadSafe>*)(CurrentRenderFrameViewports.GetData()), CurrentRenderFrameViewports.Num());
	}

	virtual void MarkComponentGeometryDirty(const FName InComponentName = NAME_None) override;
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;

	virtual class FSceneView* CalcSceneView(class ULocalPlayer* LocalPlayer, class FSceneViewFamily* ViewFamily, FVector& OutViewLocation, FRotator& OutViewRotation, class FViewport* Viewport, class FViewElementDrawer* ViewDrawer, int32 StereoViewIndex) override;

	//~~ End IDisplayClusterViewportManager

	/** Getting the viewports of the current rendering frame (viewports from the current node or from a special named list). */
	inline const TArray<TSharedPtr<FDisplayClusterViewport, ESPMode::ThreadSafe>>& ImplGetCurrentRenderFrameViewports() const
	{
		return CurrentRenderFrameViewports;
	}

	inline const TArray<TSharedPtr<FDisplayClusterViewport, ESPMode::ThreadSafe>>& ImplGetEntireClusterViewports() const
	{
		return EntireClusterViewports;
	}

	// internal use only
	bool CreateViewport(const FString& ViewportId, const class UDisplayClusterConfigurationViewport& ConfigurationViewport);
	IDisplayClusterViewport* CreateViewport(const FString& ViewportId, const TSharedPtr<IDisplayClusterProjectionPolicy, ESPMode::ThreadSafe>& InProjectionPolicy);
	bool                     DeleteViewport(const FString& ViewportId);

	TSharedPtr<FDisplayClusterViewport, ESPMode::ThreadSafe> ImplCreateViewport(const FString& ViewportId, const TSharedPtr<IDisplayClusterProjectionPolicy, ESPMode::ThreadSafe>& InProjectionPolicy);
	TSharedPtr<FDisplayClusterViewport, ESPMode::ThreadSafe> ImplFindViewport(const FString& InViewportId) const;
	void ImplDeleteViewport(const TSharedPtr<FDisplayClusterViewport, ESPMode::ThreadSafe>& InExistViewport);

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
	int32 FindFirstViewportStereoViewIndex(const TSharedPtr<FDisplayClusterViewport, ESPMode::ThreadSafe>& InViewport) const;

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

	void ResetSceneRenderTargetSize();

private:
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
	// Configuration of the current cluster node
	TUniquePtr<FDisplayClusterViewportConfiguration> Configuration;

private:
	// Manager for creating the render frame stucture
	TUniquePtr<FDisplayClusterRenderFrameManager>  RenderFrameManager;

	// Viewports of the entire cluster
	TArray<TSharedPtr<FDisplayClusterViewport, ESPMode::ThreadSafe>> EntireClusterViewports;

	// Viewports of the current render frame (viewports from the current node or from a special named list)
	TArray<TSharedPtr<FDisplayClusterViewport, ESPMode::ThreadSafe>> CurrentRenderFrameViewports;

	// Render thread proxy manager. Deleted on render thread
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

	// This VE is used for LocalPlayer::GetViewPoint(). It calls the ISceneViewExtension::SetupViewPoint() function of this VE.
	TSharedPtr<class FDisplayClusterViewportManagerViewPointExtension, ESPMode::ThreadSafe> ViewportManagerViewPointExtension;

#if WITH_EDITOR
	/** The handle for FCoreUObjectDelegates::GetPreGarbageCollectDelegate */
	FDelegateHandle PreGarbageCollectHandle;
#endif
};
