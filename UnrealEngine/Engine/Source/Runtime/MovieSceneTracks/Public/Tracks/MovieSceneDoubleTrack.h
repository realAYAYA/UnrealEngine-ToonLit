// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Tracks/MovieScenePropertyTrack.h"
#include "EntitySystem/IMovieSceneEntityProvider.h"
#include "MovieSceneDoubleTrack.generated.h"

/**
 * Handles manipulation of double properties in a movie scene
 */
UCLASS( MinimalAPI )
class UMovieSceneDoubleTrack
	: public UMovieScenePropertyTrack
{
	GENERATED_UCLASS_BODY()


public:

	/** UMovieSceneTrack interface */
	virtual bool SupportsType(TSubclassOf<UMovieSceneSection> SectionClass) const override;
	virtual UMovieSceneSection* CreateNewSection() override;
};
