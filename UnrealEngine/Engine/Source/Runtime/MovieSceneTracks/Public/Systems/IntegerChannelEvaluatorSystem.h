// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EntitySystem/MovieSceneEntitySystem.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"

#include "IntegerChannelEvaluatorSystem.generated.h"

class UObject;

namespace UE
{
namespace MovieScene
{

	struct FSourceIntegerChannel;

} // namespace MovieScene
} // namespace UE

/**
 * System that is responsible for evaluating integer channels.
 */
UCLASS()
class MOVIESCENETRACKS_API UIntegerChannelEvaluatorSystem : public UMovieSceneEntitySystem
{
public:

	GENERATED_BODY()

	UIntegerChannelEvaluatorSystem(const FObjectInitializer& ObjInit);

	virtual void OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents) override;
};
