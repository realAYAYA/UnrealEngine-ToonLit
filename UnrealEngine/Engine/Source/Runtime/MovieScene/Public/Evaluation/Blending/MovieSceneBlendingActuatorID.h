// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Evaluation/MovieSceneAnimTypeID.h"

struct FMovieSceneBlendingActuatorID : FMovieSceneAnimTypeID
{
	explicit FMovieSceneBlendingActuatorID(FMovieSceneAnimTypeID InTypeID) : FMovieSceneAnimTypeID(InTypeID) {}

	friend bool operator==(FMovieSceneBlendingActuatorID A, FMovieSceneBlendingActuatorID B)
	{
		return A.ID == B.ID;
	}

	friend bool operator!=(FMovieSceneBlendingActuatorID A, FMovieSceneBlendingActuatorID B)
	{
		return A.ID != B.ID;
	}

	friend uint32 GetTypeHash(FMovieSceneBlendingActuatorID In)
	{
		return GetTypeHash(In.ID);
	}
};
