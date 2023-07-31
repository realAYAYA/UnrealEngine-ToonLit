// Copyright Epic Games, Inc. All Rights Reserved.

#include "Systems/MovieScenePiecewiseEnumBlenderSystem.h"
#include "EntitySystem/BuiltInComponentTypes.h"
#include "EntitySystem/MovieSceneEntitySystemTask.h"
#include "EntitySystem/MovieSceneEntitySystemLinker.h"
#include "Systems/ByteChannelEvaluatorSystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MovieScenePiecewiseEnumBlenderSystem)

namespace UE
{
namespace MovieScene
{

/** 
 * Custom blend result traits for enums:
 *
 * - We don't blend enums, unlike bytes, because we don't even know if the in-between values are valid
 *   enumerations.
 * - Number of contributors doesn't matter. The last one wins.
 */
template<>
struct TSimpleBlendResultTraits<uint8>
{
	static void ZeroAccumulationBuffer(TArrayView<TSimpleBlendResult<uint8>> Buffer)
	{
		FMemory::Memzero(Buffer.GetData(), sizeof(TSimpleBlendResult<uint8>) * Buffer.Num());
	}

	static void AccumulateResult(TSimpleBlendResult<uint8>& InOutValue, uint8 Contributor)
	{
		InOutValue.Value = Contributor;
		++InOutValue.NumContributors;
	}

	static uint8 BlendResult(const TSimpleBlendResult<uint8>& InResult)
	{
		return InResult.Value;
	}
};

} // namespace MovieScene
} // namespace UE

UMovieScenePiecewiseEnumBlenderSystem::UMovieScenePiecewiseEnumBlenderSystem(const FObjectInitializer& ObjInit)
	: Super(ObjInit)
{
	Impl.Setup(
			this, 
			UE::MovieScene::FBuiltInComponentTypes::Get()->ByteResult,
			UByteChannelEvaluatorSystem::StaticClass());
}

void UMovieScenePiecewiseEnumBlenderSystem::OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents)
{
	using namespace UE::MovieScene;

	CompactBlendChannels();

	Impl.Run(Linker, AllocatedBlendChannels, InPrerequisites, Subsequents);
}


