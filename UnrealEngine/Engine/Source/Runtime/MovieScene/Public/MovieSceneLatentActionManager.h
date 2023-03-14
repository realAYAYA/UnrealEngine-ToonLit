// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Templates/Function.h"
#include "Containers/ContainersFwd.h"

class UObject;

DECLARE_DELEGATE(FMovieSceneSequenceLatentActionDelegate);

/**
 * Utility class for running latent actions created from sequence players.
 */
class MOVIESCENE_API FMovieSceneLatentActionManager
{
public:
	void AddLatentAction(FMovieSceneSequenceLatentActionDelegate Delegate);
	void ClearLatentActions(UObject* Object);
	void ClearLatentActions();

	void RunLatentActions(TFunctionRef<void()> FlushCallback);

	bool IsEmpty() const { return LatentActions.Num() == 0; }

private:
	TArray<FMovieSceneSequenceLatentActionDelegate> LatentActions;

	bool bIsRunningLatentActions = false;
};
