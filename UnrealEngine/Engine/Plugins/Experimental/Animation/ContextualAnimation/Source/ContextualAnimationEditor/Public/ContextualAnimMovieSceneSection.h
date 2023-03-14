// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MovieSceneSection.h"
#include "ContextualAnimMovieSceneSection.generated.h"

struct FContextualAnimTrack;
class UContextualAnimMovieSceneTrack;

UCLASS()
class UContextualAnimMovieSceneSection : public UMovieSceneSection
{
	GENERATED_BODY()

public:

	void Initialize(int32 InSectionIdx, int32 InAnimSetIdx, int32 InAnimTrackIdx);

	/** Returns the track owner of this section */
	UContextualAnimMovieSceneTrack& GetOwnerTrack() const;

	FContextualAnimTrack& GetAnimTrack() const;

	FORCEINLINE int32 GetSectionIdx() const { return SectionIdx; }
	FORCEINLINE int32 GetAnimSetIdx() const { return AnimSetIdx; }
	FORCEINLINE int32 GetAnimTrackIdx() const { return AnimTrackIdx; }

private:

	UPROPERTY()
	int32 SectionIdx = INDEX_NONE;

	UPROPERTY()
	int32 AnimSetIdx = INDEX_NONE;

	UPROPERTY()
	int32 AnimTrackIdx = INDEX_NONE;
};