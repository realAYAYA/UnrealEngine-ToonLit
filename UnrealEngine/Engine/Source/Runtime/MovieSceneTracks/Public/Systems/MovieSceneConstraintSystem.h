// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EntitySystem/MovieSceneEntitySystem.h"
#include "MovieSceneConstraintSystem.generated.h"

class UTransformableComponentHandle;
class UTickableTransformConstraint;

/**
 * System that is responsible for propagating constraints to a bound object's FConstraintsManagerController.
 */
UCLASS()
class MOVIESCENETRACKS_API UMovieSceneConstraintSystem : public UMovieSceneEntitySystem
{
public:

	GENERATED_BODY()
	//if constraint has a dynamic offset we need to make sure the constraint get's notified the handle is updated
	struct FUpdateHandleForConstraint
	{
		TWeakObjectPtr<UTickableTransformConstraint> Constraint;
		TWeakObjectPtr<UTransformableComponentHandle> TransformHandle;
	};

	UMovieSceneConstraintSystem(const FObjectInitializer& ObjInit);
	virtual void OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents) override;

protected:
	TArray<FUpdateHandleForConstraint>  DynamicOffsets;

};
