// Copyright Epic Games, Inc. All Rights Reserved.

#include "Systems/ObjectPathChannelEvaluatorSystem.h"
#include "EntitySystem/BuiltInComponentTypes.h"
#include "EntitySystem/MovieSceneEntitySystemTask.h"
#include "EntitySystem/MovieSceneEntitySystemLinker.h"
#include "EntitySystem/MovieSceneEvalTimeSystem.h"
#include "Channels/MovieSceneObjectPathChannel.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ObjectPathChannelEvaluatorSystem)

DECLARE_CYCLE_STAT(TEXT("MovieScene: Evaluate object path channels"), MovieSceneEval_EvaluateObjectPathChannelTask, STATGROUP_MovieSceneECS);

UObjectPathChannelEvaluatorSystem::UObjectPathChannelEvaluatorSystem(const FObjectInitializer& ObjInit)
	: Super(ObjInit)
{
	using namespace UE::MovieScene;

	FBuiltInComponentTypes* BuiltInComponents = FBuiltInComponentTypes::Get();
	RelevantComponent = BuiltInComponents->ObjectPathChannel;

	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		DefineComponentConsumer(GetClass(), BuiltInComponents->EvalTime);
		DefineComponentProducer(GetClass(), BuiltInComponents->ObjectResult);
	}
}

void UObjectPathChannelEvaluatorSystem::OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents)
{
	using namespace UE::MovieScene;

	FBuiltInComponentTypes* BuiltInComponents = FBuiltInComponentTypes::Get();

	struct FEvaluateObjectPathChannels
	{
		static void ForEachEntity(FSourceObjectPathChannel ObjectPathChannel, FFrameTime FrameTime, UObject*& OutResult)
		{
			if (!ObjectPathChannel.Source->Evaluate(FrameTime, OutResult))
			{
				OutResult = nullptr;
			}
		}
	};

	FEntityTaskBuilder()
	.Read(BuiltInComponents->ObjectPathChannel)
	.Read(BuiltInComponents->EvalTime)
	.Write(BuiltInComponents->ObjectResult)
	.FilterNone({ BuiltInComponents->Tags.Ignored })
	.SetStat(GET_STATID(MovieSceneEval_EvaluateObjectPathChannelTask))
	.Dispatch_PerEntity<FEvaluateObjectPathChannels>(&Linker->EntityManager, InPrerequisites, &Subsequents);
}

