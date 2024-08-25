// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "XRCreativeSettings.generated.h"


UENUM(BlueprintType)
enum class EXRCreativeHandedness : uint8
{
	Left	UMETA(DisplayName = "Left"),
	Right	UMETA(DisplayName = "Right"),
};


/**
 * Per project settings for XRCreative.
 */
UCLASS(Config=XRCreativeSettings, DefaultConfig, DisplayName="XR Creative")
class XRCREATIVE_API UXRCreativeSettings : public UObject
{
	GENERATED_BODY()
	
public:
	UFUNCTION(BlueprintPure, Category="XR Creative")
	static UXRCreativeSettings* GetXRCreativeSettings();
};


/**
 * Per user settings for XRCreative Editor.
 */
UCLASS(Config=EditorPerProjectUserSettings, meta=(DisplayName="XR Creative Editor"))

class XRCREATIVE_API UXRCreativeEditorSettings : public UObject
{
	GENERATED_BODY()

public:

	/** Manages Left/Right handedness user preferences.
	 * Modifying this setting requires an editor restart to take effect.
	 **/
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="XR Creative", meta=(DisplayName="Handedness"))
	EXRCreativeHandedness Handedness = EXRCreativeHandedness::Right;
	
	UFUNCTION(BlueprintPure, Category="XR Creative Editor")
	static UXRCreativeEditorSettings* GetXRCreativeEditorSettings();
};
