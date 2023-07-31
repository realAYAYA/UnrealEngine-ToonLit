// Copyright Epic Games, Inc. All Rights Reserved.

#include "Channels/MovieSceneDoublePerlinNoiseChannelContainer.h"
#include "Channels/MovieSceneDoubleChannel.h"
#include "Tracks/MovieScenePropertyTrack.h"
#include "MovieSceneTracksComponentTypes.h"
#include "Misc/Guid.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MovieSceneDoublePerlinNoiseChannelContainer)

void UMovieSceneDoublePerlinNoiseChannelContainer::InitializeOverride(FMovieSceneChannel* InChannel)
{
	PerlinNoiseChannel.PerlinNoiseParams.RandomizeOffset();
}

bool UMovieSceneDoublePerlinNoiseChannelContainer::SupportsOverride(FName DefaultChannelTypeName) const
{
	return FMovieSceneDoubleChannel::StaticStruct()->GetFName() == DefaultChannelTypeName;
}

void UMovieSceneDoublePerlinNoiseChannelContainer::ImportEntityImpl(const UE::MovieScene::FChannelOverrideEntityImportParams& OverrideParams, const UE::MovieScene::FEntityImportParams& ImportParams, UE::MovieScene::FImportedEntity* OutImportedEntity)
{
	using namespace UE::MovieScene;

	const FMovieSceneTracksComponentTypes* TracksComponents = FMovieSceneTracksComponentTypes::Get();
	const FBuiltInComponentTypes* BuiltInComponents = FBuiltInComponentTypes::Get();

	TComponentTypeID<double> ResultComponent = OverrideParams.ResultComponent.ReinterpretCast<double>();

	OutImportedEntity->AddBuilder(
		FEntityBuilder()
		.Add(TracksComponents->DoublePerlinNoiseChannel, PerlinNoiseChannel.PerlinNoiseParams)
		.Add(ResultComponent, TNumericLimits<double>::Max())
	);
}

#if WITH_EDITOR
FMovieSceneChannelHandle UMovieSceneDoublePerlinNoiseChannelContainer::AddChannelProxy(FName ChannelName, FMovieSceneChannelProxyData& ProxyData, const FMovieSceneChannelMetaData& MetaData)
{
	return ProxyData.AddWithDefaultEditorData(PerlinNoiseChannel, MetaData);
}
#else
void UMovieSceneDoublePerlinNoiseChannelContainer::AddChannelProxy(FName ChannelName, FMovieSceneChannelProxyData& ProxyData)
{
	ProxyData.Add(PerlinNoiseChannel);
}
#endif

