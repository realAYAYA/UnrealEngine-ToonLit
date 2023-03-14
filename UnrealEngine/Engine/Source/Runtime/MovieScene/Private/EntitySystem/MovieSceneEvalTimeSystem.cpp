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

struct FAssignEvalTimesTask
{
	TArray<FFrameTime>* FrameTimes;

	explicit FAssignEvalTimesTask(TArray<FFrameTime>* InFrameTimes)
		: FrameTimes(InFrameTimes)
	{}

	void ForEachEntity(UE::MovieScene::FInstanceHandle InstanceHandle, FFrameTime& EvalTime)
	{
		EvalTime = (*FrameTimes)[InstanceHandle.InstanceID];
	}
};

UMovieSceneEvalTimeSystem::UMovieSceneEvalTimeSystem(const FObjectInitializer& ObjInit)
	: Super(ObjInit)
{
	using namespace UE::MovieScene;

	RelevantComponent = FBuiltInComponentTypes::Get()->EvalTime;
	SystemCategories = EEntitySystemCategory::Core;

	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		DefineComponentProducer(GetClass(), FBuiltInComponentTypes::Get()->EvalTime);
	}
}

void UMovieSceneEvalTimeSystem::OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents)
{
	using namespace UE::MovieScene;

	SCOPE_CYCLE_COUNTER(MovieSceneEval_EvalTimeSystem)

	struct FGatherTimes
	{
		FGatherTimes(const FInstanceRegistry* InInstanceRegistry, TArray<FFrameTime>* InFrameTimes)
			: InstanceRegistry(InInstanceRegistry)
			, FrameTimes(InFrameTimes)
		{}

		const FInstanceRegistry* InstanceRegistry;
		TArray<FFrameTime>* FrameTimes;

		FORCEINLINE TStatId           GetStatId() const    { RETURN_QUICK_DECLARE_CYCLE_STAT(FGenericTask, STATGROUP_TaskGraphTasks); }
		static ENamedThreads::Type    GetDesiredThread()   { return ENamedThreads::AnyHiPriThreadHiPriTask; }
		static ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }

		void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
		{
			Run();
		}

		void Run()
		{
			FrameTimes->SetNum(InstanceRegistry->GetSparseInstances().GetMaxIndex());
			for (auto It = InstanceRegistry->GetSparseInstances().CreateConstIterator(); It; ++It)
			{
				(*FrameTimes)[It.GetIndex()] = It->GetContext().GetTime();
			}
		}
	};

	FSystemTaskPrerequisites EvalPrereqs;
	FBuiltInComponentTypes* BuiltInComponents = FBuiltInComponentTypes::Get();

	if (Linker->EntityManager.GetThreadingModel() == EEntityThreadingModel::NoThreading)
	{
		FGatherTimes(Linker->GetInstanceRegistry(), &FrameTimes).Run();
	}
	else
	{
		// The only thing we depend on is the gather task
		FGraphEventRef GatherEvalTimesEvent = TGraphTask<FGatherTimes>::CreateTask(nullptr, Linker->EntityManager.GetDispatchThread())
		.ConstructAndDispatchWhenReady(Linker->GetInstanceRegistry(), &FrameTimes);

		EvalPrereqs.AddComponentTask(BuiltInComponents->EvalTime, GatherEvalTimesEvent);
	}

	FEntityTaskBuilder()
	.Read(BuiltInComponents->InstanceHandle)
	.Write(BuiltInComponents->EvalTime)
	.FilterNone({ FBuiltInComponentTypes::Get()->Tags.FixedTime })
	.SetStat(GET_STATID(MovieSceneEval_EvalTimes))
	.Dispatch_PerEntity<FAssignEvalTimesTask>(&Linker->EntityManager, EvalPrereqs, &Subsequents, &FrameTimes);
}

