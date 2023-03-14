// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MovieSceneCopyableTrack.generated.h"

class UMovieSceneTrack;

UCLASS(Transient)
class UMovieSceneCopyableTrack : public UObject
{
public:
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UMovieSceneTrack> Track;

	UPROPERTY()
	bool bIsAMasterTrack;

	UPROPERTY()
	bool bIsACameraCutTrack;

	UPROPERTY()
	TArray<FName> FolderPath;
};
