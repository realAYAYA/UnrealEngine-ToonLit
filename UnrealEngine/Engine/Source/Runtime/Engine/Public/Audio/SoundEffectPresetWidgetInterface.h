// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

#include "AudioPanelWidgetInterface.h"
#include "Sound/SoundEffectPreset.h"
#include "UObject/Class.h"
#include "UObject/Interface.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "Templates/SubclassOf.h"

#include "SoundEffectPresetWidgetInterface.generated.h"


UINTERFACE(Blueprintable)
class ENGINE_API USoundEffectPresetWidgetInterface : public UAudioPanelWidgetInterface
{
	GENERATED_BODY()
};

class ENGINE_API ISoundEffectPresetWidgetInterface : public IAudioPanelWidgetInterface
{
	GENERATED_BODY()

public:
	// Returns the class of Preset the widget supports
	UFUNCTION(BlueprintImplementableEvent)
	TSubclassOf<USoundEffectPreset> GetClass();

	// Called when the preset widget is constructed
	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "On SoundEffectPreset Widget Constructed"))
	void OnConstructed(USoundEffectPreset* Preset);

	// Called when the preset object is changed
	UFUNCTION(BlueprintImplementableEvent)
	void OnPropertyChanged(USoundEffectPreset* Preset, FName PropertyName);
};