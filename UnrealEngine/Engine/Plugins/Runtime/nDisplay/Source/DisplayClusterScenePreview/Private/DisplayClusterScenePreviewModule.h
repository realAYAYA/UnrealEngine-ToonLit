// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDisplayClusterScenePreview.h"

#include "Containers/Ticker.h"
#include "DisplayClusterMeshProjectionRenderer.h"
#include "UObject/StrongObjectPtr.h"

class ADisplayClusterRootActor;
class ADisplayClusterLightCardActor;
class UTextureRenderTarget2D;

/**
 * Module containing tools for rendering nDisplay scene previews.
 */
class FDisplayClusterScenePreviewModule :
	public IDisplayClusterScenePreview
{
public:
	//~ IModuleInterface interface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	//~ Begin IDisplayClusterScenePreview Interface
	virtual int32 CreateRenderer() override;
	virtual bool DestroyRenderer(int32 RendererId) override;
	virtual bool SetRendererRootActorPath(int32 RendererId, const FString& ActorPath, bool bAutoUpdateLightcards = false) override;
	virtual bool SetRendererRootActor(int32 RendererId, ADisplayClusterRootActor* Actor, bool bAutoUpdateLightcards = false) override;
	virtual ADisplayClusterRootActor* GetRendererRootActor(int32 RendererId) override;
	virtual bool GetActorsInRendererScene(int32 RendererId, bool bIncludeRoot, TArray<AActor*>& OutActors) override;
	virtual bool AddActorToRenderer(int32 RendererId, AActor* Actor) override;
	virtual bool AddActorToRenderer(int32 RendererId, AActor* Actor, const TFunctionRef<bool(const UPrimitiveComponent*)>& PrimitiveFilter) override;
	virtual bool RemoveActorFromRenderer(int32 RendererId, AActor* Actor) override;
	virtual bool ClearRendererScene(int32 RendererId) override;
	virtual bool SetRendererActorSelectedDelegate(int32 RendererId, FDisplayClusterMeshProjectionRenderer::FSelection ActorSelectedDelegate) override;
	virtual bool SetRendererRenderSimpleElementsDelegate(int32 RendererId, FDisplayClusterMeshProjectionRenderer::FSimpleElementPass RenderSimpleElementsDelegate) override;
	virtual bool SetRendererUsePostProcessTexture(int32 RendererId, bool bUsePostProcessTexture) override;
	virtual bool Render(int32 RendererId, FDisplayClusterMeshProjectionRenderSettings& RenderSettings, FCanvas& Canvas) override;
	virtual bool RenderQueued(int32 RendererId, FDisplayClusterMeshProjectionRenderSettings& RenderSettings, const FIntPoint& Size, FRenderResultDelegate ResultDelegate) override;
	virtual bool RenderQueued(int32 RendererId, FDisplayClusterMeshProjectionRenderSettings& RenderSettings, const TWeakPtr<FCanvas> Canvas, FRenderResultDelegate ResultDelegate) override;
	virtual bool IsRealTimePreviewEnabled() const override;
	//~ End IDisplayClusterScenePreview Interface

private:
	/** Holds information about an active renderer created by this module. */
	struct FRendererConfig
	{
		/** The renderer itself. */
		TSharedPtr<FDisplayClusterMeshProjectionRenderer> Renderer;

		/** The root of the display cluster that this renderer is previewing. */
		TWeakObjectPtr<ADisplayClusterRootActor> RootActor;

		/** The path of the root actor that this renderer is previewing. If this is not empty and the root actor becomes invalid, we will attempt to find it again using this path. */
		FString RootActorPath;

		/** All actors that have been added to the renderer (except for the root actor). */
		TArray<TWeakObjectPtr<AActor>> AddedActors;

		/** Actors that have been automatically added to the scene. */
		TArray<TWeakObjectPtr<AActor>> AutoActors;

		/** If true, automatically update the renderer with stage actors belonging to the root actor. */
		bool bAutoUpdateLightcards = false;

		/** If true, the scene needs to be updated before the next render. This is only relevant if bAutoUpdateLightcards is true. */
		bool bIsSceneDirty = true;

		/** If true, apply post-processing to the nDisplay preview texture and override the preview component with it before rendering. */
		bool bUsePostProcessTexture = false;

		/** The render target to use for queued renders. */
		TStrongObjectPtr<UTextureRenderTarget2D> RenderTarget = nullptr;
	};

	/** Holds information about a preview render that was queued to be completed later. */
	struct FPreviewRenderJob
	{
		FPreviewRenderJob() {}

		FPreviewRenderJob(int32 RendererId, const FDisplayClusterMeshProjectionRenderSettings& Settings, const FIntPoint& Size,
			TWeakPtr<FCanvas> Canvas, FRenderResultDelegate ResultDelegate)
			: RendererId(RendererId), Settings(Settings), Size(Size), Canvas(Canvas), bWasCanvasProvided(Canvas.IsValid()), ResultDelegate(ResultDelegate)
		{
		}

		/** The ID of the renderer to use. */
		int32 RendererId;

		/** The settings to use for the render. */
		FDisplayClusterMeshProjectionRenderSettings Settings;

		/** The size of the image to render. */
		FIntPoint Size;

		/** The canvas to render to, if provided. */
		TWeakPtr<FCanvas> Canvas;

		/** Whether a canvas was provided for this job. */
		bool bWasCanvasProvided;

		/** The delegate to call when the render is completed. */
		FRenderResultDelegate ResultDelegate;
	};

	/** Get the root actor for a config. If the root actor pointer is invalid but we have a path to the actor, try to reacquire a pointer using the path first. */
	ADisplayClusterRootActor* InternalGetRendererRootActor(FRendererConfig& RendererConfig);

	/** Set the root actor for a config, update its scene, and register events accordingly. */
	void InternalSetRendererRootActor(FRendererConfig& RendererConfig, ADisplayClusterRootActor* Actor, bool bAutoUpdateLightcards);

	/** Queue a preview to be rendered. */
	bool InternalRenderQueued(int32 RendererId, FDisplayClusterMeshProjectionRenderSettings& RenderSettings, TWeakPtr<FCanvas> Canvas,
		const FIntPoint& Size, FRenderResultDelegate ResultDelegate);

	/** Immediately render with the given renderer config and settings to the given canvas. */
	bool InternalRenderImmediate(FRendererConfig& RendererConfig, FDisplayClusterMeshProjectionRenderSettings& RenderSettings, FCanvas& Canvas);

	/** Check if any of the tracked root actors are set to auto-update their lightcards and register/unregister event listeners accordingly. */
	void RegisterOrUnregisterGlobalActorEvents();

	/** Register/unregister to events affecting a cluster root actor. */
	void RegisterRootActorEvents(ADisplayClusterRootActor* Actor, bool bShouldRegister);

	/** Clear and re-populate a renderer's scene with the root actor and lightcards if applicable. */
	void AutoPopulateScene(FRendererConfig& RendererConfig);

	/** Check whether nDisplay preview textures are being updated in real time. */
	bool UpdateIsRealTimePreviewEnabled();

	/** Called on tick to process the queued renders. */
	bool OnTick(float DeltaTime);

	/** Called when a property on a root DisplayCluster actor has changed. */
	void OnActorPropertyChanged(UObject* ObjectBeingModified, FPropertyChangedEvent& PropertyChangedEvent);

	/** Called when the user deletes an actor from the level. */
	void OnLevelActorDeleted(AActor* Actor);

	/** Called when the user adds an actor to the level. */
	void OnLevelActorAdded(AActor* Actor);

	/** Called when a blueprint for an actor we care about is compiled. */
	void OnBlueprintCompiled(UBlueprint* Blueprint);

	/** Called when any object is transacted. */
	void OnObjectTransacted(UObject* Object, const FTransactionObjectEvent& TransactionObjectEvent);

private:

	/** Map from renderer ID to configuration data for that renderer. */
	TMap<int32, FRendererConfig> RendererConfigs;

	/** Queue of render jobs pending completion. */
	TQueue<FPreviewRenderJob> RenderQueue;

	/** Handle for the render ticker. */
	FTSTicker::FDelegateHandle RenderTickerHandle;

	/** The ID to use for the next created renderer. */
	int32 NextRendererId = 0;

	/** Whether this is currently registered for actor update events. */
	bool bIsRegisteredForActorEvents = false;

	/** Whether nDisplay preview textures are being updated in real time. */
	bool bIsRealTimePreviewEnabled = false;
};