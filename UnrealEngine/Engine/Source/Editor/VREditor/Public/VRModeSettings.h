// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "VISettings.h"
#include "VRModeSettings.generated.h"


class AVREditorTeleporter;
class UVREditorInteractor;
class UVREditorModeBase;


UENUM()
enum class EInteractorHand : uint8
{
	/** Right hand */
	Right,

	/** Left hand */
	Left,
};

/**
* Implements the settings for VR Mode.
*/
UCLASS(config = EditorSettings)
class VREDITOR_API UVRModeSettings : public UVISettings
{
	GENERATED_BODY()

public:

	/** Default constructor that sets up CDO properties */
	UVRModeSettings();

	/**If true, wearing a Vive or Oculus Rift headset will automatically enter VR Editing mode */
	UPROPERTY(EditAnywhere, config, Category = "General", meta = (DisplayName = "Enable VR Mode Auto-Entry"))
	uint32 bEnableAutoVREditMode : 1;

	// Whether or not sequences should be autokeyed
	UPROPERTY(EditAnywhere, config, Category = "Cinematics")
	uint32 bAutokeySequences : 1;

	// Which hand should have the primary interactor laser on it
	UPROPERTY(EditAnywhere, config, Category = "General")
	EInteractorHand InteractorHand;

	/** Show the movement grid for orientation while moving through the world */
	UPROPERTY(EditAnywhere, config, Category = "World Movement")
	uint32 bShowWorldMovementGrid : 1;

	/** Dim the surroundings while moving through the world */
	UPROPERTY(EditAnywhere, config, Category = "World Movement")
	uint32 bShowWorldMovementPostProcess : 1;

	/** Display a progress bar while scaling that shows your current scale */
	UPROPERTY(EditAnywhere, config, Category = "UI Customization")
	uint32 bShowWorldScaleProgressBar : 1;

	/** Adjusts the brightness of the UI panels */
	UPROPERTY(EditAnywhere, config, Category = "UI Customization", meta = (DisplayName = "UI Panel Brightness", ClampMin = 0.01, UIMax = 10.0))
	float UIBrightness;

	/** The size of the transform gizmo */
	UPROPERTY(EditAnywhere, config, Category = "UI Customization", meta = (ClampMin = 0.1, ClampMax = 2.0))
	float GizmoScale;

	/** The maximum time in seconds between two clicks for a double-click to register */
	UPROPERTY(EditAnywhere, config, Category = "Motion Controllers", meta = (ClampMin = 0.01, ClampMax = 1.0))
	float DoubleClickTime;

	/** The amount (between 0-1) you have to depress the Vive controller trigger to register a press */
	UPROPERTY(EditAnywhere, config, Category = "Motion Controllers", meta = (DisplayName = "Trigger Pressed Threshold (Vive)", ClampMin = 0.01, ClampMax = 1.0))
	float TriggerPressedThreshold_Vive;

	/** The amount (between 0-1) you have to depress the Oculus Touch controller trigger to register a press */
	UPROPERTY(EditAnywhere, config, Category = "Motion Controllers", meta = (DisplayName = "Trigger Pressed Threshold (Oculus Touch)", ClampMin = 0.01, ClampMax = 1.0))
	float TriggerPressedThreshold_Rift;

	UE_DEPRECATED(5.1, "Refer to UVREditorMode::InteractorClass, or create a derived mode to override the interactor class.")
	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Refer to UVREditorMode::InteractorClass, or create a derived mode to override the interactor class."))
	TSoftClassPtr<UVREditorInteractor> InteractorClass;

	UE_DEPRECATED(5.1, "Refer to UVREditorMode::TeleporterClass, or create a derived mode to override the teleporter class.")
	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Refer to UVREditorMode::TeleporterClass, or create a derived mode to override the teleporter class."))
	TSoftClassPtr<AVREditorTeleporter> TeleporterClass;

	/** The mode extension to use when UnrealEd is in VR mode. Use VREditorMode to get default editor behavior or select a custom mode. */
	UPROPERTY(EditAnywhere, config, NoClear, Category = "General")
	TSoftClassPtr<UVREditorModeBase> ModeClass;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
