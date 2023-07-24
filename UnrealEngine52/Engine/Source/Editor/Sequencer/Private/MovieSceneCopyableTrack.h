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
	bool bIsRootTrack;

	UPROPERTY()
	bool bIsCameraCutTrack;

	UPROPERTY()
	TArray<FName> FolderPath;
};
