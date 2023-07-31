// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Render/Viewport/IDisplayClusterViewport.h"
#include "Render/Viewport/RenderFrame/DisplayClusterRenderFrame.h"
#include "Render/Viewport/Containers/DisplayClusterPreviewSettings.h"
#include "SceneView.h"

class UWorld;
class FViewport;
class FSceneViewFamilyContext;
class ADisplayClusterRootActor;
class UDisplayClusterConfigurationViewport;
class IDisplayClusterViewportManagerProxy;
class IDisplayClusterViewportLightCardManager;
class FReferenceCollector;

class DISPLAYCLUSTER_API IDisplayClusterViewportManager
{
public:
	virtual ~IDisplayClusterViewportManager() = default;

public:
	virtual const IDisplayClusterViewportManagerProxy* GetProxy() const = 0;
	virtual       IDisplayClusterViewportManagerProxy* GetProxy() = 0;

	virtual UWorld* GetCurrentWorld() const = 0;

	virtual ADisplayClusterRootActor* GetRootActor() const = 0;

	/**
	* Return current scene status
	* [Game thread func]
	*/
	virtual bool IsSceneOpened() const = 0;

	/**
	* Update\Create\Delete local node viewports
	* Update ICVFX configuration from root actor components
	* [Game thread func]
	*
	* @param InRenderMode     - Render mode
	* @param InClusterNodeId  - cluster node for rendering
	* @param InRootActorPtr   - reference to RootActor with actual configuration inside
	* @param InPreviewSettings - support preview rendering
	*
	* @return - true, if success
	*/
	virtual bool UpdateConfiguration(EDisplayClusterRenderFrameMode InRenderMode, const FString& InClusterNodeId, class ADisplayClusterRootActor* InRootActorPtr, const FDisplayClusterPreviewSettings* InPreviewSettings = nullptr) = 0;

	/**
	* Update\Create\Delete viewports for frame. For rendering outside of cluster nodes
	* Update ICVFX configuration from root actor components
	* [Game thread func]
	*
	* @param InRenderMode    - Render mode
	* @param InViewportNames - Viewports names for next frame
	* @param InRootActorPtr  - reference to RootActor with actual configuration inside
	*
	* @return - true, if success
	*/
	virtual bool UpdateCustomConfiguration(EDisplayClusterRenderFrameMode InRenderMode, const TArray<FString>& InViewportNames, class ADisplayClusterRootActor* InRootActorPtr) = 0;

	/**
	* Initialize new frame for all viewports on game thread, and update context, render resources with viewport new settings
	* And finally build render frame structure and send to render thread proxy viewport objects
	* [Game thread func]
	*
	* @param InViewport          - target viewport
	* @param OutRenderFrame      - output render frame container
	*
	* @return - true, if success
	*/
	virtual bool BeginNewFrame(FViewport* InViewport, UWorld* InWorld, FDisplayClusterRenderFrame& OutRenderFrame) = 0;

	/**
	* Finalize frame logic for viewports on game thread
	* [Game thread func]
	*
	*/
	virtual void FinalizeNewFrame() = 0;

	/**
	* Create constructor values for viewfamily, using rules
	* [Game thread func]
	*
	* @param InFrameTarget           - frame target
	* @param InScene                 - scene
	* @param InEngineShowFlags       - starting showflags
	* @param bInAdditionalViewFamily - flag indicating this is an additional view family
	* 
	* @return - The ConstructionValues object that was created, meant to initialize a view family context.
	*/
	virtual FSceneViewFamily::ConstructionValues CreateViewFamilyConstructionValues(
		const FDisplayClusterRenderFrame::FFrameRenderTarget& InFrameTarget,
		FSceneInterface* InScene,
		FEngineShowFlags InEngineShowFlags,
		const bool bInAdditionalViewFamily
	) const = 0;

	/**
	* Initialize view family, using rules
	* [Game thread func]
	*
	* @param InFrameTarget          - frame target
	* @param OutRenderOutViewFamily - output family
	*
	*/
	virtual void ConfigureViewFamily(const FDisplayClusterRenderFrame::FFrameRenderTarget& InFrameTarget, const FDisplayClusterRenderFrame::FFrameViewFamily& InFrameViewFamily, FSceneViewFamilyContext& InOutViewFamily) = 0;

	// Send to render thread
	virtual void RenderFrame(FViewport* InViewport) = 0;

	virtual void AddReferencedObjects(FReferenceCollector& Collector) = 0;

#if WITH_EDITOR
	/**
	* Render in editor (preview)
	* [Game thread func]
	*
	* @param InRenderFrame - render frame setup
	* @param InViewport
	* @param InFirstViewportNum - begin render from this viewport in frame
	* @param InViewportsAmount - max viewports for render
	* @param OutViewportsAmount - total viewport rendered
	* @param bOutFrameRendered - true, if cluster node composition pass done (additional pass after last viewport is rendered)
	*
	* @return - true, if render success
	*/
	virtual bool RenderInEditor(class FDisplayClusterRenderFrame& InRenderFrame, FViewport* InViewport, const uint32 InFirstViewportNum, const int32 InViewportsAmount, int32& OutViewportsAmount, bool& bOutFrameRendered) = 0;
#endif

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/**
	* Find viewport object by name
	* [Game thread func]
	*
	* @param ViewportId - Viewport name
	*
	* @return - viewport object ref
	*/
	virtual IDisplayClusterViewport* FindViewport(const FString& InViewportId) const = 0;

	/**
	* Find viewport object and context number by stereoscopic pass index
	* [Game thread func]
	*
	* @param StereoViewIndex - stereoscopic view index
	* @param OutContextNum - context number
	*
	* @return - viewport object ref
	*/
	virtual IDisplayClusterViewport* FindViewport(const int32 StereoViewIndex, uint32* OutContextNum = nullptr) const = 0;
	
	/**
	* Return all exist viewports objects
	* [Game thread func]
	*
	* @return - arrays with viewport objects refs
	*/
	virtual const TArrayView<IDisplayClusterViewport*> GetViewports() const = 0;

	/**
	* Return the light card manager, used to manage and render UV light cards
	* [Game thread func]
	*/
	virtual TSharedPtr<IDisplayClusterViewportLightCardManager, ESPMode::ThreadSafe> GetLightCardManager() const = 0;

	/**
	* Mark the geometry of the referenced component(s) as dirty (ProceduralMesh, etc)
	* [Game thread func]
	*
	* @param InComponentName - (optional) Unique component name
	*/
	virtual void MarkComponentGeometryDirty(const FName InComponentName = NAME_None) = 0;
};

