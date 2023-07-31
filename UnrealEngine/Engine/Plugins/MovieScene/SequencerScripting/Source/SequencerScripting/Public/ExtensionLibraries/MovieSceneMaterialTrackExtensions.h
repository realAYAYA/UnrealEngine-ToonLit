// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "UObject/ObjectMacros.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "MovieSceneMaterialTrackExtensions.generated.h"

class UMovieSceneMaterialTrack;

/**
 * Function library containing methods that should be hoisted onto UMovieSceneMaterialTrack for scripting
 */
UCLASS()
class UMovieSceneMaterialTrackExtensions : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	/**
	 * Set material index of the component that is animated by the material track.
	 * @param Track The track to use
	 * @param MaterialIndex The desired material index to animate. Values of < 0 or >= NumMaterials will be silently ignored and evaluation will fail.
	 */
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Track", meta = (ScriptMethod))
	static void SetMaterialIndex(UMovieSceneComponentMaterialTrack* Track, const int32 MaterialIndex);

	/**
	 * Get material index of the component that is animated by the material track.
	 * @param Track The track to use
	 * @return The material index.
	 */
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Track", meta=(ScriptMethod))
	static int32 GetMaterialIndex(UMovieSceneComponentMaterialTrack* Track);
};
