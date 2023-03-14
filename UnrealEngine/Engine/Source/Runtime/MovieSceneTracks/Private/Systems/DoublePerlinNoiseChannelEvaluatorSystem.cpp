// Copyright Epic Games, Inc. All Rights Reserved.

#include "Systems/DoublePerlinNoiseChannelEvaluatorSystem.h"
#include "Systems/MovieScenePiecewiseDoubleBlenderSystem.h"
#include "EntitySystem/BuiltInComponentTypes.h"
#include "EntitySystem/MovieSceneEntitySystemTask.h"
#include "EntitySystem/MovieSceneEntitySystemLinker.h"
#include "EntitySystem/MovieSceneEvalTimeSystem.h"
#include "Channels/MovieSceneDoublePerlinNoiseChannel.h"
#include "MovieSceneTracksComponentTypes.h"
#include "Math/NumericLimits.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DoublePerlinNoiseChannelEvaluatorSystem)

DECLARE_CYCLE_STAT(TEXT("MovieScene: Evaluate Double Perlin Noise channels"), MovieSceneEval_EvaluateDoublePerlinNoiseChannelTask, STATGROUP_MovieSceneECS);

namespace UE
{
	namespace MovieScene
	{
		struct FEvaluateDoublePerlinNoiseChannels
		{
			FInstanceRegistry* InstanceRegistry;

			FEvaluateDoublePerlinNoiseChannels(FInstanceRegistry* InInstanceRegistry)
				: InstanceRegistry{ InInstanceRegistry }
			{
				check(InstanceRegistry);
			}

			void ForEachEntity(const FPerlinNoiseParams& PerlinNoiseParams, FInstanceHandle InstanceHandle, double& OutResult)
			{
				const FMovieSceneContext& Context = InstanceRegistry->GetContext(InstanceHandle);
				FFrameTime Time = Context.GetTime();
				FFrameRate Rate = Context.GetFrameRate();

				OutResult = FMovieSceneDoublePerlinNoiseChannel::Evaluate(PerlinNoiseParams, Rate.AsSeconds(Time));
			}
		};
	} // namespace MovieScene
} // namespace UE

UDoublePerlinNoiseChannelEvaluatorSystem::UDoublePerlinNoiseChannelEvaluatorSystem(const FObjectInitializer& ObjInit)
	: Super(ObjInit)
{
	using namespace UE::MovieScene;

	SystemCategories = EEntitySystemCategory::ChannelEvaluators;

	const FBuiltInComponentTypes* BuiltInComponents = FBuiltInComponentTypes::Get();
	const FMovieSceneTracksComponentTypes* TrackComponents = FMovieSceneTracksComponentTypes::Get();

	RelevantComponent = TrackComponents->DoublePerlinNoiseChannel;

	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		// Allow writing to all the possible channels
		for (int32 i = 0; i < UE_ARRAY_COUNT(BuiltInComponents->DoubleResult); ++i)
		{
			DefineComponentProducer(UDoublePerlinNoiseChannelEvaluatorSystem::StaticClass(), BuiltInComponents->DoubleResult[i]);
		}

		DefineImplicitPrerequisite(UMovieSceneEvalTimeSystem::StaticClass(), GetClass());
		DefineImplicitPrerequisite(GetClass(), UMovieScenePiecewiseDoubleBlenderSystem::StaticClass());
	}
}

void UDoublePerlinNoiseChannelEvaluatorSystem::OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents)
{
	using namespace UE::MovieScene;

	const FBuiltInComponentTypes* BuiltInComponents = FBuiltInComponentTypes::Get();
	const FMovieSceneTracksComponentTypes* TrackComponents = FMovieSceneTracksComponentTypes::Get();

	for (int32 i = 0; i < UE_ARRAY_COUNT(BuiltInComponents->DoubleResult); ++i)
	{
		FEntityTaskBuilder()
			.Read(TrackComponents->DoublePerlinNoiseChannel)
			.Read(BuiltInComponents->InstanceHandle)
			.Write(BuiltInComponents->DoubleResult[i])
			.FilterNone({ BuiltInComponents->Tags.Ignored })
			.SetStat(GET_STATID(MovieSceneEval_EvaluateDoublePerlinNoiseChannelTask))
			.Dispatch_PerEntity<FEvaluateDoublePerlinNoiseChannels>(&Linker->EntityManager, InPrerequisites, &Subsequents, GetLinker()->GetInstanceRegistry());
	}
}

