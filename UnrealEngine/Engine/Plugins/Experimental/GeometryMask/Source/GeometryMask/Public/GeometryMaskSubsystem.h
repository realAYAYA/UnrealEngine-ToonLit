// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/World.h"
#include "GeometryMaskCanvas.h"
#include "GeometryMaskCanvasResource.h"
#include "SceneView.h"
#include "Subsystems/EngineSubsystem.h"
#include "Subsystems/WorldSubsystem.h"

#include "GeometryMaskSubsystem.generated.h"

using FOnGeometryMaskResourceCreated = TMulticastDelegate<void(const UGeometryMaskCanvasResource*)>;
using FOnGeometryMaskResourceDestroyed = TMulticastDelegate<void(const UGeometryMaskCanvasResource*)>;

/** Maintains the registered named canvases. */
UCLASS(BlueprintType)
class GEOMETRYMASK_API UGeometryMaskSubsystem
	: public UEngineSubsystem
{
	GENERATED_BODY()

public:
	/** Returns the default, blank canvas. */
	UFUNCTION(BlueprintCallable, Category = "Canvas")
	UGeometryMaskCanvas* GetDefaultCanvas();

	int32 GetNumCanvasResources() const;

	const TSet<TObjectPtr<UGeometryMaskCanvasResource>>& GetCanvasResources() const;

	void Update(UWorld* InWorld, FSceneViewFamily& InViewFamily);

	/** Toggles if no arg given. */
	void ToggleUpdate(const TOptional<bool>& bInShouldUpdate = {});

	/** Called when a new canvas resource is created. */
	FOnGeometryMaskResourceCreated& OnGeometryMaskResourceCreated() { return OnGeometryMaskResourceCreatedDelegate; }

	/** Called when a canvas resource is destroyed. */
	FOnGeometryMaskResourceDestroyed& OnGeometryMaskResourceDestroyed() { return OnGeometryMaskResourceDestroyedDelegate; }

private:	
	/** Find and assign the next available resource to the given canvas. */
	void AssignResourceToCanvas(UGeometryMaskCanvas* InCanvas);

	/** Re-arrange used canvases such that they use as few Canvas Resource's as possible.
	 *  Note that this could cause momentary visual artifacts.
	 *  This won't check for unused canvas's, only resource channels. */
	void CompactResources();

	void OnWorldDestroyed(UWorld* InWorld);

private:
	friend class UGeometryMaskWorldSubsystem;

	FOnGeometryMaskResourceCreated OnGeometryMaskResourceCreatedDelegate;
	FOnGeometryMaskResourceDestroyed OnGeometryMaskResourceDestroyedDelegate;

	std::atomic<bool> bDoUpdates = true;

	UPROPERTY()
	TObjectPtr<UGeometryMaskCanvas> DefaultCanvas;

	/** Pool of GPU/Texture resources used by the canvases. */
	UPROPERTY(Getter)
	TSet<TObjectPtr<UGeometryMaskCanvasResource>> CanvasResources;
};
