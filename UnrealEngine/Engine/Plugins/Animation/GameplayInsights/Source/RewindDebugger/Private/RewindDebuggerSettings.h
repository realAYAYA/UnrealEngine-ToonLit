// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "RewindDebuggerSettings.generated.h"

class URewindDebuggerExtensionSettings;

UENUM()
enum class ERewindDebuggerCameraMode
{
	Replay UMETA(Tooltip="Replay Recorded Camera"),
	FollowTargetActor UMETA(Tooltip="Follow Target Actor"),
	Disabled UMETA(Tooltip="Disable Camera On Playback"),
};

/**
 * Implements the settings for the Rewind Debugger.
 */
UCLASS(Config=Editor, meta=(DisplayName="Rewind Debugger"))
class URewindDebuggerSettings : public UDeveloperSettings
{
	GENERATED_BODY()
	
public:
	URewindDebuggerSettings();

#if WITH_EDITOR
	virtual FText GetSectionText() const override;
	virtual FText GetSectionDescription() const override;
#endif

	virtual FName GetCategoryName() const override;
	
	/** Rewind Debugger Playback Camera Mode */
	UPROPERTY(EditAnywhere, Config, Category = Camera)
	ERewindDebuggerCameraMode CameraMode;
	
	// /** If enabled, automatically detach player controller at the start of PIE */
	// UPROPERTY(EditAnywhere, Config, Category = Other, meta = (DisplayName = "Auto Detach Player Controller on PIE"))
	// bool bShouldAutoDetach;
	
	/** If enabled, start recording information at the start of PIE */
	UPROPERTY(EditAnywhere, Config, Category = Other)
	bool bShouldAutoRecordOnPIE;

	/** If enabled, show empty tracks on Rewind Debugger Timeline*/
	UPROPERTY(EditAnywhere, Config, Category = Filters)
	bool bShowEmptyObjectTracks;
	
	/** Get Mutable CDO of URewindDebuggerSettings */
	static URewindDebuggerSettings & Get();
};
