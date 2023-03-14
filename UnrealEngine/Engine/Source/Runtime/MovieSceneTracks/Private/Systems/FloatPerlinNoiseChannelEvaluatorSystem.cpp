// Copyright Epic Games, Inc. All Rights Reserved.

#include "Systems/FloatPerlinNoiseChannelEvaluatorSystem.h"
#include "Systems/MovieScenePiecewiseDoubleBlenderSystem.h"
#include "EntitySystem/BuiltInComponentTypes.h"
#include "EntitySystem/MovieSceneEntitySystemTask.h"
#include "EntitySystem/MovieSceneEntitySystemLinker.h"
#include "EntitySystem/MovieSceneEvalTimeSystem.h"
#include "Channels/MovieSceneFloatPerlinNoiseChannel.h"
#include "MovieSceneTracksComponentTypes.h"
#include "Math/NumericLimits.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(FloatPerlinNoiseChannelEvaluatorSystem)

DECLARE_CYCLE_STAT(TEXT("MovieScene: Evaluate Float Perlin Noise channels"), MovieSceneEval_EvaluateFloatPerlinNoiseChannelTask, STATGROUP_MovieSceneECS);

namespace UE
{
	namespace MovieScene
	{
		struct FEvaluateFloatPerlinNoiseChannels
		{
			FInstanceRegistry* InstanceRegistry;

			FEvaluateFloatPerlinNoiseChannels(FInstanceRegistry* InInstanceRegistry)
				: InstanceRegistry{ InInstanceRegistry }
			{
				check(InstanceRegistry);
			}

			void ForEachEntity(const FPerlinNoiseParams& PerlinNoiseParams, FInstanceHandle InstanceHandle, double& OutResult)
			{
				const FMovieSceneContext& Context = InstanceRegistry->GetContext(InstanceHandle);
				FFrameTime Time = Context.GetTime();
				FFrameRate Rate = Context.GetFrameRate();

				const float NoiseResult = FMovieSceneFloatPerlinNoiseChannel::Evaluate(PerlinNoiseParams, Rate.AsSeconds(Time));
				OutResult = (double)NoiseResult;
			}
		};
	} // namespace MovieScene
} // namespace UE

UFloatPerlinNoiseChannelEvaluatorSystem::UFloatPerlinNoiseChannelEvaluatorSystem(const FObjectInitializer& ObjInit)
	: Super(ObjInit)
{
	using namespace UE::MovieScene;

	SystemCategories = EEntitySystemCategory::ChannelEvaluators;

	const FBuiltInComponentTypes* BuiltInComponents = FBuiltInComponentTypes::Get();
	const FMovieSceneTracksComponentTypes* TrackComponents = FMovieSceneTracksComponentTypes::Get();

	RelevantComponent = TrackComponents->FloatPerlinNoiseChannel;

	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		// Allow writing to all the possible channels
		for (int32 i = 0; i < UE_ARRAY_COUNT(BuiltInComponents->DoubleResult); ++i)
		{
			DefineComponentProducer(UFloatPerlinNoiseChannelEvaluatorSystem::StaticClass(), BuiltInComponents->DoubleResult[i]);
		}

		DefineImplicitPrerequisite(UMovieSceneEvalTimeSystem::StaticClass(), GetClass());
		DefineImplicitPrerequisite(GetClass(), UMovieScenePiecewiseDoubleBlenderSystem::StaticClass());
	}
}

void UFloatPerlinNoiseChannelEvaluatorSystem::OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents)
{
	using namespace UE::MovieScene;

	const FBuiltInComponentTypes* BuiltInComponents = FBuiltInComponentTypes::Get();
	const FMovieSceneTracksComponentTypes* TrackComponents = FMovieSceneTracksComponentTypes::Get();

	for (int32 i = 0; i < UE_ARRAY_COUNT(BuiltInComponents->DoubleResult); ++i)
	{
		FEntityTaskBuilder()
			.Read(TrackComponents->FloatPerlinNoiseChannel)
			.Read(BuiltInComponents->InstanceHandle)
			.Write(BuiltInComponents->DoubleResult[i])
			.FilterNone({ BuiltInComponents->Tags.Ignored })
			.SetStat(GET_STATID(MovieSceneEval_EvaluateFloatPerlinNoiseChannelTask))
			.Dispatch_PerEntity<FEvaluateFloatPerlinNoiseChannels>(&Linker->EntityManager, InPrerequisites, &Subsequents, GetLinker()->GetInstanceRegistry());
	}
}

