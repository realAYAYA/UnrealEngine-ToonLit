// Copyright Epic Games, Inc. All Rights Reserved.

#include "Systems/BoolChannelEvaluatorSystem.h"
#include "EntitySystem/BuiltInComponentTypes.h"
#include "EntitySystem/MovieSceneEntitySystemTask.h"
#include "EntitySystem/MovieSceneEntitySystemLinker.h"

#include "EntitySystem/MovieSceneEvalTimeSystem.h"
#include "Channels/MovieSceneBoolChannel.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BoolChannelEvaluatorSystem)

DECLARE_CYCLE_STAT(TEXT("MovieScene: Evaluate bool channels"), MovieSceneEval_EvaluateBoolChannelTask, STATGROUP_MovieSceneECS);

UBoolChannelEvaluatorSystem::UBoolChannelEvaluatorSystem(const FObjectInitializer& ObjInit)
	: Super(ObjInit)
{
	using namespace UE::MovieScene;

	SystemCategories = EEntitySystemCategory::ChannelEvaluators;
	RelevantComponent = FBuiltInComponentTypes::Get()->BoolChannel;

	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		DefineImplicitPrerequisite(UMovieSceneEvalTimeSystem::StaticClass(), GetClass());
	}
}

void UBoolChannelEvaluatorSystem::OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents)
{
	using namespace UE::MovieScene;

	FBuiltInComponentTypes* BuiltInComponents = FBuiltInComponentTypes::Get();

	struct FEvaluateBoolChannels
	{
		static void ForEachEntity(FSourceBoolChannel BoolChannel, FFrameTime FrameTime, bool& OutResult)
		{
			if (!BoolChannel.Source->Evaluate(FrameTime, OutResult))
			{
				OutResult = false;
			}
		}
	};

	FEntityTaskBuilder()
	.Read(BuiltInComponents->BoolChannel)
	.Read(BuiltInComponents->EvalTime)
	.Write(BuiltInComponents->BoolResult)
	.FilterNone({ BuiltInComponents->Tags.Ignored })
	.SetStat(GET_STATID(MovieSceneEval_EvaluateBoolChannelTask))
	.Dispatch_PerEntity<FEvaluateBoolChannels>(&Linker->EntityManager, InPrerequisites, &Subsequents);
}

