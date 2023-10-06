// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EntitySystem/MovieSceneEntityIDs.h"
#include "EntitySystem/MovieScenePropertyMetaDataTraits.h"
#include "EntitySystem/MovieScenePropertySystemTypes.h"
#include "EntitySystem/MovieScenePropertyTraits.h"
#include "Internationalization/Text.h"

struct FMovieSceneTextChannel;

namespace UE::MovieScene
{

/** The component data for evaluating a Text channel */
struct FSourceTextChannel
{
	FSourceTextChannel() : Source(nullptr) {}
	FSourceTextChannel(const FMovieSceneTextChannel* InSource) : Source(InSource) {}
	const FMovieSceneTextChannel* Source;
};

using FTextPropertyTraits = TDirectPropertyTraits<FText>;

struct FTextComponentTypes
{
	MOVIESCENETEXTTRACK_API static FTextComponentTypes* Get();
	MOVIESCENETEXTTRACK_API static void Destroy();

	// An FMovieSceneTextChannel
	TComponentTypeID<FSourceTextChannel> TextChannel;

	// Result of an evaluated FMovieSceneTextChannel
	TComponentTypeID<FText> TextResult;

	TPropertyComponents<FTextPropertyTraits> Text;

private:
	FTextComponentTypes();
};

}