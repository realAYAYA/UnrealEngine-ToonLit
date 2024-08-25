// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"

#include "GeometryMaskCanvasActor.generated.h"

class UGeometryMaskCanvas;
class IGeometryMaskWriteInterface;
class UGeometryMaskCaptureComponent;
class UGeometryMaskWriteMeshComponent;

/** Wraps a GeometryMaskCanvas, and discovers/registers writers. */
UCLASS(BlueprintType)
class GEOMETRYMASK_API AGeometryMaskCanvasActor
	: public AActor
{
	GENERATED_BODY()

public:
	AGeometryMaskCanvasActor(const FObjectInitializer& ObjectInitializer);

	/** Identifies the referenced Canvas. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Canvas")
	FName CanvasName;

	/** Returns the Canvas Texture. */
	UFUNCTION(BlueprintCallable, Category = "Rendering")
	UCanvasRenderTarget2D* GetTexture();

	virtual void BeginPlay() override;

	virtual void PostLoad() override;

#if WITH_EDITOR
	virtual void RerunConstructionScripts() override;
#endif

protected:
	/** Resolve/locate the canvas identified by CanvasName. */
	bool TryResolveCanvas();

	/** Find writers on child actors and add them to the referenced canvas. */
	void FindWriters();

	/** Reference to the Canvas used, identified by CanvasName. */
	UPROPERTY(Transient, DuplicateTransient)
	TObjectPtr<UGeometryMaskCanvas> Canvas;

	/** List of objects that write to this canvas. */
	UPROPERTY()
	TArray<TScriptInterface<IGeometryMaskWriteInterface>> Writers;
};
