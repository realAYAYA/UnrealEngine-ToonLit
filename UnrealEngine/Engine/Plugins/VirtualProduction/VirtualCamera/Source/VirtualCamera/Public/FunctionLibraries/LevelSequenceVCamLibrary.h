// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UObject/ObjectPtr.h"
#include "LevelSequenceVCamLibrary.generated.h"

class UCameraComponent;
class ULevelSequence;
class UObject;

USTRUCT(BlueprintType)
struct FPilotableSequenceCameraInfo
{
	GENERATED_BODY()

	/** A camera from a sequencer track */
	UPROPERTY(BlueprintReadOnly, Category = "Virtual Camera | Level Sequence")
	TObjectPtr<UCameraComponent> Camera;
};

/**
 * Utility functions for Level Sequences to implement VCamHUD UI.
 */
UCLASS()
class VIRTUALCAMERA_API ULevelSequenceVCamLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:

	/**
	 * Checks whether this sequence has any camera cuts set up.
	 *
	 * If yes, you should pilot the sequence using ULevelSequenceEditorBlueprintLibrary::SetLockCameraCutToViewport instead of directly piloting
	 * by using FindLevelSequencePilotableCamerasInActiveLevelSequence.
	 */
	UFUNCTION(BlueprintPure, Category = "Virtual Camera | Level Sequence", meta = (WorldContext = "WorldContextObject"))
	static bool HasAnyCameraCutsInLevelSequence(ULevelSequence* Sequence);

	/**
	 * Gets all cameras currently spawned by the active level sequence.
	 * Note: You must have called ULevelSequenceEditorBlueprintLibrary::OpenLevelSequence before calling this function.
	 * Note: Only works in the editor.
	 */
	UFUNCTION(BlueprintPure, Category = "Virtual Camera | Level Sequence")
	static TArray<FPilotableSequenceCameraInfo> FindPilotableCamerasInActiveLevelSequence();
};
