// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DeveloperSettings.h"
#include "HAL/Platform.h"
#include "UObject/NameTypes.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"

#include "AudioEditorSettings.generated.h"

class UObject;
struct FPropertyChangedEvent;

UCLASS(config = EditorPerProjectUserSettings, defaultconfig, meta = (DisplayName = "Audio"))
class AUDIOEDITOR_API UAudioEditorSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** Whether to pin the Sound Cue asset type when creating new assets. */
	UPROPERTY(EditAnywhere, config, Category = AssetMenu)
	bool bPinSoundCueInAssetMenu = true;

	/** Whether to pin the Sound Cue Template asset type when creating new assets. */
	UPROPERTY(EditAnywhere, config, Category = AssetMenu)
	bool bPinSoundCueTemplateInAssetMenu = false;

	/** Whether to pin the Sound Attenuation asset type when creating new assets. */
	UPROPERTY(EditAnywhere, config, Category = AssetMenu)
	bool bPinSoundAttenuationInAssetMenu = true;

	/** Whether to pin the Sound Concurrency asset type when creating new assets. */
	UPROPERTY(EditAnywhere, config, Category = AssetMenu)
	bool bPinSoundConcurrencyInAssetMenu = true;

	//~ Begin UDeveloperSettings
	virtual FName GetCategoryName() const override { return TEXT("General"); }
	//~ End UDeveloperSettings

	//~ Begin UObject
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	//~ End UObject
};

