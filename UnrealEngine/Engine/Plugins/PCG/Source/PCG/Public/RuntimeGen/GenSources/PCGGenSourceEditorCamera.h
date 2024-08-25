// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGGenSourceBase.h"

#include "PCGGenSourceEditorCamera.generated.h"

#if WITH_EDITOR
class FEditorViewportClient;
#endif

/**
 * This GenerationSource captures active Editor Viewports per tick to provoke RuntimeGeneration. Editor Viewports
 * are not captured by default, but can be enabled on the PCGWorldActor via bTreatEditorViewportAsGenerationSource.
 */
UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCG_API UPCGGenSourceEditorCamera : public UObject, public IPCGGenSourceBase
{
	GENERATED_BODY()

public:
	/** Returns the world space position of this gen source. */
	virtual TOptional<FVector> GetPosition() const override;

	/** Returns the normalized forward vector of this gen source. */
	virtual TOptional<FVector> GetDirection() const override;

public:
#if WITH_EDITORONLY_DATA
	FEditorViewportClient* EditorViewportClient = nullptr;
#endif
};
