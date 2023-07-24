// Copyright Epic Games, Inc. All Rights Reserved.

#include "EntitySystem/MovieSceneEvalTimeSystem.h"
#include "EntitySystem/MovieSceneEntityManager.h"
#include "EntitySystem/MovieSceneEntitySystemTask.h"
#include "EntitySystem/MovieSceneEntitySystemLinker.h"
#include "EntitySystem/BuiltInComponentTypes.h"
#include "EntitySystem/MovieSceneInstanceRegistry.h"
#include "Stats/Stats2.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MovieSceneEvalTimeSystem)


DECLARE_CYCLE_STAT(TEXT("EvalTime System"), MovieSceneEval_EvalTimeSystem, STATGROUP_MovieSceneECS);
DECLARE_CYCLE_STAT(TEXT("MovieScene: Assign evaluation times"), MovieSceneEval_EvalTimes, STATGROUP_MovieSceneECS);

UMovieSceneEvalTimeSystem::UMovieSceneEvalTimeSystem(const FObjectInitializer& ObjInit)
	: Super(ObjInit)
{
	using namespace UE::MovieScene;

	const FBuiltInComponentTypes* BuiltInComponents = FBuiltInComponentTypes::Get();

	SystemCategories = EEntitySystemCategory::Core;
	RelevantFilter.Any({ BuiltInComponents->EvalTime, BuiltInComponents->EvalSeconds });

	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		DefineComponentProducer(GetClass(), BuiltInComponents->EvalTime);
		DefineComponentProducer(GetClass(), BuiltInComponents->EvalSeconds);
	}
}

bool UMovieSceneEvalTimeSystem::IsRelevantImpl(UMovieSceneEntitySystemLinker* InLinker) const
{
	return InLinker->EntityManager.Contains(RelevantFilter);
}

void UMovieSceneEvalTimeSystem::OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents)
{
	using namespace UE::MovieScene;

	SCOPE_CYCLE_COUNTER(MovieSceneEval_EvalTimeSystem)

	struct FGatherTimes
	{
		FGatherTimes(const FInstanceRegistry* InInstanceRegistry, TArray<FEvaluatedTime>* InEvaluatedTimes)
			: InstanceRegistry(InInstanceRegistry)
			, EvaluatedTimes(InEvaluatedTimes)
		{}

		const FInstanceRegistry* InstanceRegistry;
		TArray<FEvaluatedTime>* EvaluatedTimes;

		FORCEINLINE TStatId           GetStatId() const    { RETURN_QUICK_DECLARE_CYCLE_STAT(FGenericTask, STATGROUP_TaskGraphTasks); }
		static ENamedThreads::Type    GetDesiredThread()   { return ENamedThreads::AnyHiPriThreadHiPriTask; }
		static ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }

		void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
		{
			Run();
		}

		void Run()
		{
			EvaluatedTimes->SetNum(InstanceRegistry->GetSparseInstances().GetMaxIndex());
			for (auto It = InstanceRegistry->GetSparseInstances().CreateConstIterator(); It; ++It)
			{
				const FMovieSceneContext& Context = It->GetContext();

				FEvaluatedTime EvaluatedTime;
				EvaluatedTime.FrameTime = Context.GetTime();
				EvaluatedTime.Seconds = Context.GetFrameRate().AsSeconds(EvaluatedTime.FrameTime);

				(*EvaluatedTimes)[It.GetIndex()] = EvaluatedTime;
			}
		}
	};

	struct FAssignEvalTimesTask
	{
		const TArray<FEvaluatedTime>* EvaluatedTimes;

		explicit FAssignEvalTimesTask(const TArray<FEvaluatedTime>* InEvaluatedTimes)
			: EvaluatedTimes(InEvaluatedTimes)
		{}

		void ForEachEntity(UE::MovieScene::FInstanceHandle InstanceHandle, FFrameTime& EvalTime)
		{
			EvalTime = (*EvaluatedTimes)[InstanceHandle.InstanceID].FrameTime;
		}
	};

	struct FAssignEvalSecondsTask
	{
		const TArray<FEvaluatedTime>* EvaluatedTimes;

		explicit FAssignEvalSecondsTask(const TArray<FEvaluatedTime>* InEvaluatedTimes)
			: EvaluatedTimes(InEvaluatedTimes)
		{}

		void ForEachEntity(UE::MovieScene::FInstanceHandle InstanceHandle, double& EvalSeconds)
		{
			EvalSeconds = (*EvaluatedTimes)[InstanceHandle.InstanceID].Seconds;
		}
	};

	FSystemTaskPrerequisites EvalPrereqs;
	FBuiltInComponentTypes* BuiltInComponents = FBuiltInComponentTypes::Get();

	if (Linker->EntityManager.GetThreadingModel() == EEntityThreadingModel::NoThreading)
	{
		FGatherTimes(Linker->GetInstanceRegistry(), &EvaluatedTimes).Run();
	}
	else
	{
		// The only thing we depend on is the gather task
		FGraphEventRef GatherEvalTimesEvent = TGraphTask<FGatherTimes>::CreateTask(nullptr, Linker->EntityManager.GetDispatchThread())
		.ConstructAndDispatchWhenReady(Linker->GetInstanceRegistry(), &EvaluatedTimes);

		EvalPrereqs.AddComponentTask(BuiltInComponents->EvalTime, GatherEvalTimesEvent);
		EvalPrereqs.AddComponentTask(BuiltInComponents->EvalSeconds, GatherEvalTimesEvent);
	}

	FEntityTaskBuilder()
	.Read(BuiltInComponents->InstanceHandle)
	.Write(BuiltInComponents->EvalTime)
	.FilterNone({ FBuiltInComponentTypes::Get()->Tags.FixedTime })
	.SetStat(GET_STATID(MovieSceneEval_EvalTimes))
	.Dispatch_PerEntity<FAssignEvalTimesTask>(&Linker->EntityManager, EvalPrereqs, &Subsequents, &EvaluatedTimes);

	FEntityTaskBuilder()
	.Read(BuiltInComponents->InstanceHandle)
	.Write(BuiltInComponents->EvalSeconds)
	.FilterNone({ FBuiltInComponentTypes::Get()->Tags.FixedTime })
	.SetStat(GET_STATID(MovieSceneEval_EvalTimes))
	.Dispatch_PerEntity<FAssignEvalSecondsTask>(&Linker->EntityManager, EvalPrereqs, &Subsequents, &EvaluatedTimes);
}

