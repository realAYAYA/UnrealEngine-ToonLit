// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CineCameraComponent.h"
#include "AssetRegistry/AssetData.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Misc/FrameRate.h"
#include "Misc/Timecode.h"
#include "MovieSceneObjectBindingID.h"
#include "VCamBlueprintFunctionLibrary.generated.h"

class ACineCameraActor;
class UCineCameraComponent;
class ULevelSequence;
class USceneCaptureComponent2D;
class UVirtualCameraClipsMetaData;
class UVirtualCameraUserSettings; 

UCLASS(config=VirtualCamera, BlueprintType)
class VIRTUALCAMERA_API UVCamBlueprintFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	/** Returns true if not in editor or if running the game in PIE or Simulate*/
	UFUNCTION(BlueprintPure, Category = "VirtualCamera")
	static bool IsGameRunning();

	UFUNCTION(BlueprintPure, Category = "VirtualCamera")
	static UVirtualCameraUserSettings* GetUserSettings();

	/** Get the currently opened level sequence asset */
	UFUNCTION(BlueprintPure, Category = "VirtualCamera")
	static ULevelSequence* GetCurrentLevelSequence();

	/** Play the current level sequence */
	UFUNCTION(BlueprintCallable, Category = "VirtualCamera")
	static void PlayCurrentLevelSequence();

	/** Pause the current level sequence */
	UFUNCTION(BlueprintCallable, Category = "VirtualCamera")
	static void PauseCurrentLevelSequence();

	/** Set playback position for the current level sequence in frames */
	UFUNCTION(BlueprintCallable, Category = "VirtualCamera")
	static void SetCurrentLevelSequenceCurrentFrame(int32 NewFrame);

	/** Get the current playback position in frames */
	UFUNCTION(BlueprintPure, Category = "VirtualCamera")
	static int32 GetCurrentLevelSequenceCurrentFrame();

	/** Get length in frames of a level sequence */
	UFUNCTION(BlueprintCallable, Category = "VirtualCamera")
	static int32 GetLevelSequenceLengthInFrames(const ULevelSequence* LevelSequence);

	/** Convert a frame from a level sequence to timecode */
	UFUNCTION(BlueprintCallable, Category = "VirtualCamera")
	static FTimecode GetLevelSequenceFrameAsTimecode(const ULevelSequence* LevelSequence, int32 InFrame);

	/** Convert a frame from a level sequence to timecode using only a provided display rate */
	UFUNCTION(BlueprintCallable, Category = "VirtualCamera")
	static FTimecode GetLevelSequenceFrameAsTimecodeWithoutObject(const FFrameRate DisplayRate, int32 InFrame);

	/** Check whether the sequence is actively playing. */
	UFUNCTION(BlueprintPure, Category = "VirtualCamera")
	static bool IsCurrentLevelSequencePlaying();

	/** Imports image as a uasset */
	UFUNCTION(BlueprintCallable, Category = "VirtualCamera")
	static UTexture* ImportSnapshotTexture(FString FileName, FString SubFolderName, FString AbsolutePathPackage);

	/** Saves UVirtualCameraClipsMetaData with updated selects information. */ 
	UFUNCTION(BlueprintCallable, Category = "VirtualCamera | Metadata")
	static bool ModifyLevelSequenceMetadataForSelects(UVirtualCameraClipsMetaData* LevelSequenceMetaData, bool bIsSelected);
	
	/** Marks a LevelSequence as dirty and saves it, persisting metadata changes */
	UFUNCTION(BlueprintCallable, Category = "VirtualCamera | Metadata")
	static bool ModifyLevelSequenceMetadata(UVirtualCameraClipsMetaData* LevelSequenceMetaData);
	
	/** Save an asset through path. Returns true on success. */
	UFUNCTION(BlueprintCallable, Category = "VirtualCamera | Clips")
	static bool EditorSaveAsset(FString AssetPath);

	/** Load an asset through path. */
	UFUNCTION(BlueprintCallable, Category = "VirtualCamera | Clips")
	static UObject* EditorLoadAsset(FString AssetPath);

	/** Modifies a UObject's metadata tags, adding a tag if the tag does not exist. */
	UFUNCTION(BlueprintCallable, Category = "VirtualCamera | Clips")
	static void ModifyObjectMetadataTags(UObject* InObject, FName InTag, FString InValue);
	
	/** Retrieves UObject's metadata tags */
	UFUNCTION(BlueprintCallable, Category = "VirtualCamera | Clips")
	static TMap<FName, FString> GetObjectMetadataTags(UObject* InObject); 

	/** Sort array of FAssetData by metadata timecode **/
	UFUNCTION(BlueprintCallable, Category = "VirtualCamera | Clips ")
	static TArray<FAssetData> SortAssetsByTimecodeAssetData(TArray<FAssetData> LevelSequenceAssets);

	/** Pilot the provided actor using editor scripting */
	UFUNCTION(BlueprintCallable, Category = "VirtualCamera | Streaming ")
	static void PilotActor(AActor* SelectedActor);

	/** Updates the provided USceneCaptureComponent2D's PostProcessingSettings. Returns true on success. */
	UFUNCTION(BlueprintCallable, Category = "VirtualCamera")
	static bool UpdatePostProcessSettingsForCapture(USceneCaptureComponent2D* CaptureComponent, float DepthOfField, float FStopValue);

	/** Grab the display rate from a LevelSequences' MovieScene */
	UFUNCTION(BlueprintCallable, Category = "VirtualCamera")
	static FFrameRate GetDisplayRate(ULevelSequence* LevelSequence);

	/** Converts a double framerate to a FFrameRate */
	UFUNCTION(BlueprintCallable, Category = "VirtualCamera")
	static FFrameRate ConvertStringToFrameRate(FString InFrameRateString);

	/** Returns true if the function was found & executed correctly. */
	UFUNCTION(BlueprintCallable, Category = "VirtualCamera")
	static bool CallFunctionByName(UObject* ObjPtr, FName FunctionName);

	/** Sets the current game view */
	UFUNCTION(BlueprintCallable, Category = "VirtualCamera")
	static void EditorSetGameView(bool bIsToggled);

	/** Calculates auto focus */
	UFUNCTION(BlueprintCallable, Category = "VirtualCamera")
	static float CalculateAutoFocusDistance(FVector2D ReticlePosition, UCineCameraComponent* CineCamera);

	/** Get UObject from Camera Object Bindings*/
	UFUNCTION(BlueprintCallable, Category = "VirtualCamera")
	static TArray<UObject*> GetBoundObjects(FMovieSceneObjectBindingID CameraBindingID);

	/** Enable/Disable debug focus plane*/
	UFUNCTION(BlueprintCallable, Category = "VirtualCamera")
	static void EnableDebugFocusPlane(UCineCameraComponent* CineCamera, bool bEnabled);

	/** Convert timecode to amount of frames at a given framerate */
	UFUNCTION(BlueprintCallable, Category = "VirtualCamera")
	static int32 TimecodeToFrameAmount(FTimecode Timecode, const FFrameRate& InFrameRate);

	/** Returns the description of the undo action that will be performed next.*/
	UFUNCTION(BlueprintPure, Category = "VirtualCamera")
	static FString GetNextUndoDescription();

	/** Copies all properties from a CineCameraComponent to a CineCameraActor and ensure the root actor transform is updated so the CameraComponents end up in the same World Space position */
	UFUNCTION(BlueprintCallable, Category = "VirtualCamera")
	static bool CopyToCineCameraActor(UCineCameraComponent* SourceCameraComponent, ACineCameraActor* TargetCameraActor);

	UFUNCTION(BlueprintCallable, Category = "VirtualCamera")
	static void SetActorLabel(AActor* Actor, const FString& NewActorLabel);

	UFUNCTION(BlueprintPure, Category = "VirtualCamera")
	static bool IsTakeRecorderPanelOpen();

	UFUNCTION(BlueprintCallable, Category = "VirtualCamera")
	static bool TryOpenTakeRecorderPanel();
private:

	static bool DeprojectScreenToWorld(const FVector2D& InScreenPosition, FVector& OutWorldPosition, FVector& OutWorldDirection);
};