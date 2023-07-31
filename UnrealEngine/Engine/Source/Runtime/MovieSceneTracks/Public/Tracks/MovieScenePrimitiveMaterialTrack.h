// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneTrack.h"
#include "Tracks/MovieScenePropertyTrack.h"
#include "MovieScenePrimitiveMaterialTrack.generated.h"

UCLASS(MinimalAPI)
class UMovieScenePrimitiveMaterialTrack : public UMovieScenePropertyTrack
{
public:

	GENERATED_BODY()

	UMovieScenePrimitiveMaterialTrack(const FObjectInitializer& ObjInit);

	/* Set the material index that this track is assigned to */
	MOVIESCENETRACKS_API void SetMaterialIndex(int32 MaterialIndex);

	/* Get the material index that this track is assigned to */
	MOVIESCENETRACKS_API int32 GetMaterialIndex() const;

	/*~ UMovieSceneTrack interface */
	virtual UMovieSceneSection* CreateNewSection() override;
	virtual bool SupportsType(TSubclassOf<UMovieSceneSection> SectionClass) const override;

private:
	UPROPERTY()
	int32 MaterialIndex;
};
