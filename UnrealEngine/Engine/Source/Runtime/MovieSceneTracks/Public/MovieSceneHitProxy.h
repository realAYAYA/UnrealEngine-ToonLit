// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HitProxies.h"
#include "Sections/MovieScene3DTransformSection.h"
#include "Tracks/MovieScene3DTransformTrack.h"
#include "MovieSceneTrack.h"

struct HMovieSceneKeyProxy : public HHitProxy
{
	DECLARE_HIT_PROXY( MOVIESCENETRACKS_API );

	/** The track that contains the section */
	TWeakObjectPtr<UMovieSceneTrack> MovieSceneTrack;
	/** The trajectory key data */
	FTrajectoryKey Key;

	HMovieSceneKeyProxy(class UMovieSceneTrack* InTrack, const FTrajectoryKey& InKey) :
		HHitProxy(HPP_UI),
		MovieSceneTrack(InTrack),
		Key(InKey)
	{}

	virtual EMouseCursor::Type GetMouseCursor() override
	{
		return EMouseCursor::Crosshairs;
	}
};

