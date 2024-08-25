// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DeveloperSettings.h"
#include "HAL/Platform.h"
#include "UObject/NameTypes.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "AudioDefines.h"
#include "AudioEditorSettings.generated.h"

class UObject;
struct FPropertyChangedEvent;

UCLASS(config = EditorPerProjectUserSettings, defaultconfig, meta = (DisplayName = "Audio"))
class AUDIOEDITOR_API UAudioEditorSettings : public UDeveloperSettings
{
	GENERATED_BODY()

protected:

	/** Whether or not should Audio Attenuation be used by default, for Non-Game Worlds*/
	UPROPERTY(EditAnywhere, config, Category = NonGameWorld)
	bool bUseAudioAttenuation = true;

public:
	/** Whether to pin the Sound Cue asset type when creating new assets. Requires editor restart to take effect. */
	UPROPERTY(EditAnywhere, config, Category = AssetMenu)
	bool bPinSoundCueInAssetMenu = true;

	/** Whether to pin the Sound Cue Template asset type when creating new assets. Requires editor restart to take effect. */
	UPROPERTY(EditAnywhere, config, Category = AssetMenu)
	bool bPinSoundCueTemplateInAssetMenu = false;

	/** Whether to pin the Sound Attenuation asset type when creating new assets. Requires editor restart to take effect. */
	UPROPERTY(EditAnywhere, config, Category = AssetMenu)
	bool bPinSoundAttenuationInAssetMenu = true;

	/** Whether to pin the Sound Concurrency asset type when creating new assets. Requires editor restart to take effect. */
	UPROPERTY(EditAnywhere, config, Category = AssetMenu)
	bool bPinSoundConcurrencyInAssetMenu = true;

	/** Set and apply, whether audio attenuation is used for non-game worlds*/
	void SetUseAudioAttenuation(bool bInUseAudioAttenuation);

	/** Is audio attenuation used for non-game worlds*/
	bool IsUsingAudioAttenuation() const { return bUseAudioAttenuation; };

	//~ Begin UDeveloperSettings
	virtual FName GetCategoryName() const override { return TEXT("General"); }
	//~ End UDeveloperSettings

	//~ Begin UObject
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostInitProperties();
	//~ End UObject

private:

	/** Apply non-game world attenuation setting for all AudioDevices.*/
	void ApplyAttenuationForAllAudioDevices();

	/** Apply non-game world attenuation setting for AudioDevice with given ID.*/
	void ApplyAttenuationForAudioDevice(Audio::FDeviceId InDeviceID);
};
