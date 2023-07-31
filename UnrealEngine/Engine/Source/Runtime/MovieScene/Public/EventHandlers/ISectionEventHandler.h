// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/List.h"
#include "CoreTypes.h"
#include "EventHandlers/MovieSceneDataEventContainer.h"

class UMovieSceneSection;

namespace UE
{
namespace MovieScene
{

class ISectionEventHandler
{
public:

	virtual void OnRowChanged(UMovieSceneSection*) {}
};

} // namespace MovieScene
} // namespace UE

