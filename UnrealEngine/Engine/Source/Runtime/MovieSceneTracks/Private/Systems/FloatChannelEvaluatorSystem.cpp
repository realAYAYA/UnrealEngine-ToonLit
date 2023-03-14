// Copyright Epic Games, Inc. All Rights Reserved.

#include "Systems/FloatChannelEvaluatorSystem.h"
#include "EntitySystem/BuiltInComponentTypes.h"
#include "EntitySystem/EntityAllocationIterator.h"
#include "EntitySystem/MovieSceneEntitySystemTask.h"
#include "EntitySystem/MovieSceneEntitySystemLinker.h"
#include "EntitySystem/MovieSceneComponentRegistry.h"

#include "EntitySystem/MovieSceneEvalTimeSystem.h"
#include "Channels/MovieSceneFloatChannel.h"
#include "MovieSceneTracksComponentTypes.h"

#include "Algo/Find.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(FloatChannelEvaluatorSystem)

DECLARE_CYCLE_STAT(TEXT("MovieScene: Float channel system"), MovieSceneEval_FloatChannelSystem, STATGROUP_MovieSceneECS);
DECLARE_CYCLE_STAT(TEXT("MovieScene: Gather float channels"), MovieSceneEval_GatherFloatChannelTask, STATGROUP_MovieSceneECS);
DECLARE_CYCLE_STAT(TEXT("MovieScene: Evaluate float channels"), MovieSceneEval_EvaluateFloatChannelTask, STATGROUP_MovieSceneECS);

namespace UE
{
namespace MovieScene
{

// @todo: for multi-bindings we currently re-evaluate the float channel for each binding, even though the time is the same.
// Do we need to optimize for this case using something like the code below, while pessimizing the common (non-multi-bind) codepath??
struct FEvaluateFloatChannels
{
	void ForEachEntity(FSourceFloatChannel FloatChannel, FFrameTime FrameTime, double& OutResult, FSourceFloatChannelFlags& OutFlags)
	{
		if (OutFlags.bNeedsEvaluate == false)
		{
			return;
		}

		float FloatResult = 0.f;
		if (!FloatChannel.Source->Evaluate(FrameTime, FloatResult))
		{
			FloatResult = MIN_flt;
		}
		OutResult = (double)FloatResult;

		if (FloatChannel.Source->GetTimes().Num() <= 1)
		{
			OutFlags.bNeedsEvaluate = false;
		}
	}
};


} // namespace MovieScene
} // namespace UE


TArray<UFloatChannelEvaluatorSystem::FChannelType, TInlineAllocator<16>> UFloatChannelEvaluatorSystem::StaticChannelTypes;

UFloatChannelEvaluatorSystem::UFloatChannelEvaluatorSystem(const FObjectInitializer& ObjInit)
	: Super(ObjInit)
{
	using namespace UE::MovieScene;

	SystemCategories = EEntitySystemCategory::ChannelEvaluators;

	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		DefineImplicitPrerequisite(UMovieSceneEvalTimeSystem::StaticClass(), GetClass());

		FBuiltInComponentTypes* Components = FBuiltInComponentTypes::Get();

		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Components->FloatChannel); ++Index)
		{
			RegisterChannelType(Components->FloatChannel[Index], Components->FloatChannelFlags[Index], Components->DoubleResult[Index]);
		}
		RegisterChannelType(Components->WeightChannel, Components->WeightChannelFlags, Components->WeightResult);
	}
}

void UFloatChannelEvaluatorSystem::RegisterChannelType(TComponentTypeID<UE::MovieScene::FSourceFloatChannel> SourceChannelType, TComponentTypeID<UE::MovieScene::FSourceFloatChannelFlags> ChannelFlagsType, TComponentTypeID<double> ResultType)
{
	using namespace UE::MovieScene;

	FChannelType ChannelType;
	ChannelType.ChannelType = SourceChannelType;
	ChannelType.ChannelFlagsType = ChannelFlagsType;
	ChannelType.ResultType  = ResultType;

	DefineComponentProducer(UFloatChannelEvaluatorSystem::StaticClass(), ResultType);

	StaticChannelTypes.Add(ChannelType);
}

bool UFloatChannelEvaluatorSystem::IsRelevantImpl(UMovieSceneEntitySystemLinker* InLinker) const
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

void UFloatChannelEvaluatorSystem::OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents)
{
	using namespace UE::MovieScene;

	SCOPE_CYCLE_COUNTER(MovieSceneEval_FloatChannelSystem)

	FBuiltInComponentTypes* BuiltInComponents = FBuiltInComponentTypes::Get();

	for (const FChannelType& ChannelType : StaticChannelTypes)
	{
		// Evaluate float channels per instance and write the evaluated value into the output
		FEntityTaskBuilder()
		.Read(ChannelType.ChannelType)
		.Read(BuiltInComponents->EvalTime)
		.Write(ChannelType.ResultType)
		.Write(ChannelType.ChannelFlagsType)
		.FilterNone({ BuiltInComponents->Tags.Ignored })
		.SetStat(GET_STATID(MovieSceneEval_EvaluateFloatChannelTask))
		.Dispatch_PerEntity<FEvaluateFloatChannels>(&Linker->EntityManager, InPrerequisites, &Subsequents);
	}
}

