// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneTrack.h"
#include "Tracks/MovieScenePropertyTrack.h"
#include "MovieSceneObjectPropertyTrack.generated.h"

UCLASS(MinimalAPI)
class UMovieSceneObjectPropertyTrack : public UMovieScenePropertyTrack
{
public:

	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UClass> PropertyClass;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	bool bClassProperty = false;
#endif

	UMovieSceneObjectPropertyTrack(const FObjectInitializer& ObjInit);

	/*~ UMovieSceneTrack interface */
	virtual bool SupportsType(TSubclassOf<UMovieSceneSection> SectionClass) const override;
	virtual UMovieSceneSection* CreateNewSection() override;
};
