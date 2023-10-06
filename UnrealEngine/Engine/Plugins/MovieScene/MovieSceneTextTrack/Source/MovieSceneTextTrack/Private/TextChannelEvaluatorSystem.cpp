// Copyright Epic Games, Inc. All Rights Reserved.

#include "TextChannelEvaluatorSystem.h"
#include "EntitySystem/BuiltInComponentTypes.h"
#include "EntitySystem/MovieSceneEntitySystemLinker.h"
#include "EntitySystem/MovieSceneEntitySystemTask.h"
#include "EntitySystem/MovieSceneEvalTimeSystem.h"
#include "MovieSceneTextChannel.h"
#include "TextComponentTypes.h"

DECLARE_CYCLE_STAT(TEXT("MovieScene: Evaluate text channels"), MovieSceneEval_EvaluateTextChannelTask, STATGROUP_MovieSceneECS);

namespace UE::MovieScene::Private
{
	struct FEvaluateTextChannels
	{
		static void ForEachEntity(FSourceTextChannel TextChannel, FFrameTime FrameTime, FText& OutResult)
		{
			if (const FText* Value = TextChannel.Source->Evaluate(FrameTime))
			{
				OutResult = *Value;
			}
			else
			{
				OutResult = FText::GetEmpty();
			}
		}
	};
}

UTextChannelEvaluatorSystem::UTextChannelEvaluatorSystem(const FObjectInitializer& InObjectInitializer)
	: Super(InObjectInitializer)
{
	using namespace UE::MovieScene;

	SystemCategories  = EEntitySystemCategory::ChannelEvaluators;
	RelevantComponent = FTextComponentTypes::Get()->TextChannel;
	Phase             = ESystemPhase::Scheduling;

	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		DefineImplicitPrerequisite(UMovieSceneEvalTimeSystem::StaticClass(), GetClass());
	}
}

void UTextChannelEvaluatorSystem::OnSchedulePersistentTasks(UE::MovieScene::IEntitySystemScheduler* TaskScheduler)
{
	using namespace UE::MovieScene;

	const FBuiltInComponentTypes* BuiltInComponents = FBuiltInComponentTypes::Get();
	const FTextComponentTypes* TextComponents = FTextComponentTypes::Get();

	FEntityTaskBuilder()
		.Read(TextComponents->TextChannel)
		.Read(BuiltInComponents->EvalTime)
		.Write(TextComponents->TextResult)
		.FilterNone({ BuiltInComponents->Tags.Ignored })
		.SetStat(GET_STATID(MovieSceneEval_EvaluateTextChannelTask))
		.Fork_PerEntity<Private::FEvaluateTextChannels>(&Linker->EntityManager, TaskScheduler);
}

void UTextChannelEvaluatorSystem::OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents)
{
	using namespace UE::MovieScene;

	const FBuiltInComponentTypes* BuiltInComponents = FBuiltInComponentTypes::Get();
	const FTextComponentTypes* TextComponents = FTextComponentTypes::Get();

	FEntityTaskBuilder()
		.Read(TextComponents->TextChannel)
		.Read(BuiltInComponents->EvalTime)
		.Write(TextComponents->TextResult)
		.FilterNone({ BuiltInComponents->Tags.Ignored })
		.SetStat(GET_STATID(MovieSceneEval_EvaluateTextChannelTask))
		.Dispatch_PerEntity<Private::FEvaluateTextChannels>(&Linker->EntityManager, InPrerequisites, &Subsequents);
}
