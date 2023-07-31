// Copyright Epic Games, Inc. All Rights Reserved.

#include "Systems/DoubleChannelEvaluatorSystem.h"
#include "EntitySystem/BuiltInComponentTypes.h"
#include "EntitySystem/EntityAllocationIterator.h"
#include "EntitySystem/MovieSceneEntitySystemTask.h"
#include "EntitySystem/MovieSceneEntitySystemLinker.h"
#include "EntitySystem/MovieSceneComponentRegistry.h"

#include "EntitySystem/MovieSceneEvalTimeSystem.h"
#include "Channels/MovieSceneDoubleChannel.h"

#include "Algo/Find.h"
#include "Math/NumericLimits.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DoubleChannelEvaluatorSystem)

DECLARE_CYCLE_STAT(TEXT("MovieScene: Double channel system"), MovieSceneEval_DoubleChannelSystem, STATGROUP_MovieSceneECS);
DECLARE_CYCLE_STAT(TEXT("MovieScene: Gather double channels"), MovieSceneEval_GatherDoubleChannelTask, STATGROUP_MovieSceneECS);
DECLARE_CYCLE_STAT(TEXT("MovieScene: Evaluate double channels"), MovieSceneEval_EvaluateDoubleChannelTask, STATGROUP_MovieSceneECS);

namespace UE
{
namespace MovieScene
{

// @todo: for multi-bindings we currently re-evaluate the double channel for each binding, even though the time is the same.
// Do we need to optimize for this case using something like the code below, while pessimizing the common (non-multi-bind) codepath??
struct FEvaluateDoubleChannels
{
	void ForEachEntity(FSourceDoubleChannel DoubleChannel, FFrameTime FrameTime, double& OutResult, FSourceDoubleChannelFlags& OutFlags)
	{
		if (OutFlags.bNeedsEvaluate == false)
		{
			return;
		}

		if (!DoubleChannel.Source->Evaluate(FrameTime, OutResult))
		{
			OutResult = MIN_dbl;
		}

		if (DoubleChannel.Source->GetTimes().Num() <= 1)
		{
			OutFlags.bNeedsEvaluate = false;
		}
	}
};

} // namespace MovieScene
} // namespace UE


TArray<UDoubleChannelEvaluatorSystem::FChannelType, TInlineAllocator<4>> UDoubleChannelEvaluatorSystem::StaticChannelTypes;

UDoubleChannelEvaluatorSystem::UDoubleChannelEvaluatorSystem(const FObjectInitializer& ObjInit)
	: Super(ObjInit)
{
	using namespace UE::MovieScene;

	SystemCategories = EEntitySystemCategory::ChannelEvaluators;

	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		DefineImplicitPrerequisite(UMovieSceneEvalTimeSystem::StaticClass(), GetClass());

		FBuiltInComponentTypes* Components = FBuiltInComponentTypes::Get();

		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Components->DoubleChannel); ++Index)
		{
			RegisterChannelType(Components->DoubleChannel[Index], Components->DoubleChannelFlags[Index], Components->DoubleResult[Index]);
		}
	}
}

void UDoubleChannelEvaluatorSystem::RegisterChannelType(TComponentTypeID<UE::MovieScene::FSourceDoubleChannel> SourceChannelType, TComponentTypeID<UE::MovieScene::FSourceDoubleChannelFlags> ChannelFlagsType, TComponentTypeID<double> ResultType)
{
	using namespace UE::MovieScene;

	FChannelType ChannelType;
	ChannelType.ChannelType = SourceChannelType;
	ChannelType.ChannelFlagsType = ChannelFlagsType;
	ChannelType.ResultType  = ResultType;

	DefineComponentProducer(UDoubleChannelEvaluatorSystem::StaticClass(), ResultType);

	StaticChannelTypes.Add(ChannelType);
}

bool UDoubleChannelEvaluatorSystem::IsRelevantImpl(UMovieSceneEntitySystemLinker* InLinker) const
{
	for (const FChannelType& ChannelType : StaticChannelTypes)
	{
		if (InLinker->EntityManager.ContainsComponent(ChannelType.ChannelType))
		{
			return true;
		}
	}

	return false;
}

void UDoubleChannelEvaluatorSystem::OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents)
{
	using namespace UE::MovieScene;

	SCOPE_CYCLE_COUNTER(MovieSceneEval_DoubleChannelSystem)

	FBuiltInComponentTypes* BuiltInComponents = FBuiltInComponentTypes::Get();

	for (const FChannelType& ChannelType : StaticChannelTypes)
	{
		// Evaluate double channels per instance and write the evaluated value into the output
		FEntityTaskBuilder()
		.Read(ChannelType.ChannelType)
		.Read(BuiltInComponents->EvalTime)
		.Write(ChannelType.ResultType)
		.Write(ChannelType.ChannelFlagsType)
		.FilterNone({ BuiltInComponents->Tags.Ignored })
		.SetStat(GET_STATID(MovieSceneEval_EvaluateDoubleChannelTask))
		.Dispatch_PerEntity<FEvaluateDoubleChannels>(&Linker->EntityManager, InPrerequisites, &Subsequents);
	}
}

