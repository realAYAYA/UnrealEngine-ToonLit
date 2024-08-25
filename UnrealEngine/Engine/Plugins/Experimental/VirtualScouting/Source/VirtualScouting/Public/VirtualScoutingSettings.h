// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DeveloperSettingsBackedByCVars.h"
#include "VirtualScoutingSettings.generated.h"


/**
 * Per project settings for Virtual Scouting.
 */
UCLASS(Config=VirtualScoutingSettings, DefaultConfig, DisplayName="Virtual Scouting")
class VIRTUALSCOUTING_API UVirtualScoutingSettings : public UObject
{
	GENERATED_BODY()
	
public:

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Virtual Scouting", meta=(DisplayName="Show Measurements in Imperial Units"))
	bool bUseImperial = false;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Virtual Scouting", meta=(DisplayName="Viewfinder Use AutoExposure"))
	bool bViewfinderUseExposure = false;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Virtual Scouting", meta=(DisplayName="Viewfinder ExposureCompensation", ClampMin=-15, ClampMax=15))
	float ViewfinderExposureCompensation = 1;
	
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Virtual Scouting", meta=(DisplayName="Viewfinder Apertures"))
	TArray<float> ViewfinderApertureArray = {1.2, 2.0, 2.8, 4.0, 5.6, 8.0, 11.0, 16.0, 22.0};

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Virtual Scouting", meta=(DisplayName="Viewfinder Monitor Masks"))
	TArray<float> ViewfinderMaskArray = {1.33, 1.66, 1.78, 2.0, 2.35, 2.39 };

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Virtual Scouting", meta=(DisplayName="Sequence Tool Collection"))
	FName SequenceToolCollection;
	
	UFUNCTION(BlueprintPure, Category="Virtual Scouting", DisplayName="Virtual Scouting Settings")
	static UVirtualScoutingSettings* GetVirtualScoutingSettings();
};

/**
 * Per user settings for Virtual Scouting Editor.
 */
UCLASS(Config=EditorPerProjectUserSettings, DisplayName="Virtual Scouting Editor Settings")
class VIRTUALSCOUTING_API UVirtualScoutingEditorSettings : public UObject
{
	GENERATED_BODY()
	
public:

	UFUNCTION(BlueprintPure, Category="Virtual Scouting Editor")
	static UVirtualScoutingEditorSettings* GetVirtualScoutingEditorSettings();

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Virtual Scouting User", meta=(DisplayName= "Flight Speed", ToolTip="Avatar flying speed. Default is 4.0", ClampMin=1.0f, ClampMax=10.0f));
	float FlightSpeed = 4.0f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Virtual Scouting User", meta=(DisplayName= "Drag Speed", ToolTip="Speed of movement when you drag-move. Default is 0.7", ClampMin=0.1f, ClampMax=2.0f));
	float DragSpeed = 0.7f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Virtual Scouting User", meta=(DisplayName= "Show Tooltips", ToolTip="Show Tooltips when in VR. These appear when motioncontroller is brought near to HMD"));
	bool bEnableTooltips = true;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Virtual Scouting User", meta=(DisplayName= "Use Smooth Rotation", ToolTip="True = Rotate smoothly. False = Flick Rotate. Default is Flick Rotate"));
	bool bUseSmoothRotation = false;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Virtual Scouting User", meta=(DisplayName= "Use Teleport Rotation", ToolTip="Use the forward axis roll from the motion controller to define and adjust teleport rotation"));
	bool bUseTeleportRotation = false;
	
};
