// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "UObject/UObjectGlobals.h"
#include "VCamUserSettings.generated.h"

UENUM(BlueprintType)
enum class EVCamTutorialCompletionState : uint8
{
	/** Tutorial not completed */
	Pending,
	/** Tutorial was completed */
	Completed
};

UCLASS(BlueprintType, Config = EditorPerProjectUserSettings)
class VIRTUALCAMERA_API UVirtualCameraUserSettings : public UObject
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintPure, Category = "Virtual Camera", meta = (Keywords = "Get Settings Virtual Camera VCam"))
	static UVirtualCameraUserSettings* GetSettings() { return GetMutableDefault<UVirtualCameraUserSettings>(); }

	/**
	 * Indicates whether the VCam tutorial is completed.
	 * You can manually reset this to Pending if you want to retake the tutorial.
	 * The tutorial shown in the default VCamHUD, e.g. to teach gestures.
	 */
	UPROPERTY(Config, BlueprintReadWrite, EditAnywhere, BlueprintSetter = "SetTutorialCompletionState", Category = "Virtual Camera")
	EVCamTutorialCompletionState VCamTutorialCompletionState = EVCamTutorialCompletionState::Pending;

	/** @return Whether the VCam tutorial has been completed. */
	UFUNCTION(BlueprintPure, Category = "Virtual Camera")
	bool IsTutorialCompleted() const { return VCamTutorialCompletionState == EVCamTutorialCompletionState::Completed; }

	/** Sets the tutorial completion state and saves it to the config file if it has changed. */
	UFUNCTION(BlueprintCallable, Category = "Virtual Camera")
	void SetTutorialCompletionState(EVCamTutorialCompletionState NewState)
	{
		if (VCamTutorialCompletionState != NewState)
		{
			VCamTutorialCompletionState = NewState;
			SaveConfig();
		}
	}
};