// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SoundScapeModule.h"
#include "SoundscapeColor.h"
#include "SoundScapePalette.h"
#include "SoundScape.generated.h"

UCLASS()
class SOUNDSCAPE_API USoundscapeBPFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

	/** Spawns a new Soundscape Elemental Agent. Returns true if spawn was successful. */
	UFUNCTION(BlueprintCallable, Category = "Soundscape", meta = (WorldContext = "WorldContextObject"))
	static bool SpawnSoundscapeColor(UObject* WorldContextObject, class USoundscapeColor* SoundscapeColorIn, UActiveSoundscapeColor*& ActiveSoundscapeColor);

	/** Spawns a new Soundscape Palette Agent. Returns true if spawn was successful. */
	UFUNCTION(BlueprintCallable, Category = "Soundscape", meta = (WorldContext = "WorldContextObject"))
	static bool SpawnSoundscapePalette(UObject* WorldContextObject, class USoundscapePalette* SoundscapePaletteIn, UActiveSoundscapePalette*& SoundscapePaletteAgentsOut);

};
