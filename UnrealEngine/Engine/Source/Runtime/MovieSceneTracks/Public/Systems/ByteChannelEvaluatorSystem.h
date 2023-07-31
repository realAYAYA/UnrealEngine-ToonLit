// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EntitySystem/MovieSceneEntitySystem.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"

#include "ByteChannelEvaluatorSystem.generated.h"

class UObject;

namespace UE
{
namespace MovieScene
{

	struct FSourceByteChannel;

} // namespace MovieScene
} // namespace UE

/**
 * System that is responsible for evaluating byte channels.
 */
UCLASS()
class MOVIESCENETRACKS_API UByteChannelEvaluatorSystem : public UMovieSceneEntitySystem
{
public:

	GENERATED_BODY()

	UByteChannelEvaluatorSystem(const FObjectInitializer& ObjInit);

	virtual void OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents) override;
};
