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
UCLASS(config = EditorPerProjectUserSettings, meta=(DisplayName="Rewind Debugger"))
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
	
	/** If enabled, automatically detach player control when PIE is paused */
	UPROPERTY(EditAnywhere, Config, Category = Other)
	bool bShouldAutoEject;
	
	/** If enabled, start recording information at the start of PIE */
	UPROPERTY(EditAnywhere, Config, Category = Other)
	bool bShouldAutoRecordOnPIE;

	/** If enabled, show empty tracks on Rewind Debugger Timeline*/
	UPROPERTY(EditAnywhere, Config, Category = Filters)
	bool bShowEmptyObjectTracks;
	
	/** The track types listed here will be hidden from the track tree view */
	UPROPERTY(EditAnywhere, Config, Category = Filters)
	TArray<FName> HiddenTrackTypes;

	/** The track types listed here will be hidden from the track tree view */
	UPROPERTY(Config)
	FString DebugTargetActor;
	
	/** Get Mutable CDO of URewindDebuggerSettings */
	static URewindDebuggerSettings & Get();
};
