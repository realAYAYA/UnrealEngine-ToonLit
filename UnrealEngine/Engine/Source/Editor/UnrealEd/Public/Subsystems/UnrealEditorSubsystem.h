// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"

#include "UnrealEditorSubsystem.generated.h"

/**
* UUnrealEditorSubsystem 
* Subsystem for exposing editor functionality to scripts
*/
UCLASS(MinimalAPI)
class UUnrealEditorSubsystem  : public UEditorSubsystem
{
	GENERATED_BODY()

public:

	/**
	 * Gets information about the camera position for the primary level editor viewport.  In non-editor builds, these will be zeroed
	 * In the UnrealEd module instead of Level Editor as it uses FLevelEditorViewportClient which is in this module
	 *
	 * @param	CameraLocation	(out) Current location of the level editing viewport camera, or zero if none found
	 * @param	CameraRotation	(out) Current rotation of the level editing viewport camera, or zero if none found
	 * @return	Whether or not we were able to get a camera for a level editing viewport
	 */
	UFUNCTION(BlueprintPure, Category = "Development|Editor")
	UNREALED_API bool GetLevelViewportCameraInfo(FVector& CameraLocation, FRotator& CameraRotation);

	/**
	* Sets information about the camera position for the primary level editor viewport.
	* In the UnrealEd module instead of Level Editor as it uses FLevelEditorViewportClient which is in this module
	*
	* @param	CameraLocation	Location the camera will be moved to.
	* @param	CameraRotation	Rotation the camera will be set to.
	*/
	UFUNCTION(BlueprintCallable, Category = "Development|Editor")
	UNREALED_API void SetLevelViewportCameraInfo(FVector CameraLocation, FRotator CameraRotation);

	/**
	 * Find the World in the world editor. It can then be used as WorldContext by other libraries like GameplayStatics.
	 * @return	The World used by the world editor.
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Level Utility")
	UNREALED_API UWorld* GetEditorWorld();

	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Level Utility")
	UNREALED_API UWorld* GetGameWorld();
};
