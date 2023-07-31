// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Interface.h"
#include "UObject/ObjectMacros.h"
#include "Engine/EngineTypes.h"
#include "MovieSceneSequenceID.h"
#include "IMovieScenePlaybackClient.generated.h"


/** Movie scene binding overrides interface */
UINTERFACE()
class MOVIESCENE_API UMovieScenePlaybackClient
	: public UInterface
{
public:
	GENERATED_BODY()
};


class MOVIESCENE_API IMovieScenePlaybackClient
{
public:
	GENERATED_BODY()

	/**
	 * Locate bound objects that relate to the specified binding ID
	 *
	 * @param InBindingId 		The GUID of the object binding in the movie scene
	 * @param OutObjects 		Array to populate with bound objects
	 * @return true to prevent default lookup of the binding in the sequence itself, else false
	 */
	virtual bool RetrieveBindingOverrides(const FGuid& InBindingId, FMovieSceneSequenceID InSequenceID, TArray<UObject*, TInlineAllocator<1>>& OutObjects) const = 0;

	/**
	 * Retrieve the optional instance data that should be used for this evaluation
	 */
	virtual UObject* GetInstanceData() const = 0;

	/**
	 * Whether this playback client wants a specific aspect ratio axis constraint during playback.
	 */
	virtual TOptional<EAspectRatioAxisConstraint> GetAspectRatioAxisConstraint() const
	{
		return TOptional<EAspectRatioAxisConstraint>();
	}

	/*
	 * Whether this playback client wants replicated playback.
	 */
	virtual bool GetIsReplicatedPlayback() const
	{
		return false;
	}
};
