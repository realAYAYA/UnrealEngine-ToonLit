// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GeometryMaskSubsystem.h"
#include "Subsystems/WorldSubsystem.h"
#include "UnrealClient.h"

#include "GeometryMaskWorldSubsystem.generated.h"

class FGeometryMaskSceneViewExtension;

using FOnGeometryMaskCanvasCreated = TMulticastDelegate<void(const UGeometryMaskCanvas*)>;
using FOnGeometryMaskCanvasDestroyed = TMulticastDelegate<void(const FGeometryMaskCanvasId&)>;



/** Updates the canvases. */
UCLASS()
class GEOMETRYMASK_API UGeometryMaskWorldSubsystem
	: public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Retrieves a Canvas, uniquely identified by this world and the canvas name. */
	UFUNCTION(BlueprintCallable, Category = "Canvas")
	UGeometryMaskCanvas* GetNamedCanvas(FName InName);

	/** Returns all registered canvas names for this world. */
	UFUNCTION(BlueprintCallable, Category = "Canvas")
	TArray<FName> GetCanvasNames();
	
	/** Remove all canvases without any Readers or Writers. Return the number of canvases removed. */
	int32 RemoveWithoutWriters();
	
	/** Called when a new canvas is created due to a unique name being requested. */
	FOnGeometryMaskCanvasCreated& OnGeometryMaskCanvasCreated() { return OnGeometryMaskCanvasCreatedDelegate; }

	/** Called when a canvas is destroyed due to having no registered writers. */
	FOnGeometryMaskCanvasDestroyed& OnGeometryMaskCanvasDestroyed() { return OnGeometryMaskCanvasDestroyedDelegate; }

protected:
	// ~Begin USubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	// ~End USubsystem

private:
	void OnCanvasActivated(UGeometryMaskCanvas* InCanvas);
	void OnCanvasDeactivated(UGeometryMaskCanvas* InCanvas);

private:
	friend class UGeometryMaskSubsystem;
	
	TSharedPtr<FGeometryMaskSceneViewExtension> GeometryMaskSceneViewExtension;

	FOnGeometryMaskCanvasCreated OnGeometryMaskCanvasCreatedDelegate;
	FOnGeometryMaskCanvasDestroyed OnGeometryMaskCanvasDestroyedDelegate;

	UPROPERTY()
	TMap<FName, TObjectPtr<UGeometryMaskCanvas>> NamedCanvases;
};
