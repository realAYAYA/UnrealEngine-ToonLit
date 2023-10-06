// Copyright Epic Games, Inc. All Rights Reserved.

#include "MovieSceneHitProxy.h"
#include "GenericPlatform/ICursor.h"

IMPLEMENT_HIT_PROXY(HMovieSceneKeyProxy, HHitProxy);

HMovieSceneKeyProxy::HMovieSceneKeyProxy(class UMovieSceneTrack* InTrack, const FTrajectoryKey& InKey)
	: HHitProxy(HPP_UI)
	, MovieSceneTrack(InTrack)
	, Key(InKey)
{}

EMouseCursor::Type HMovieSceneKeyProxy::GetMouseCursor()
{
	return EMouseCursor::Crosshairs;
}
