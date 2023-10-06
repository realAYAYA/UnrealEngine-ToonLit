// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Channels/MovieSceneChannelOverrideContainer.h"
#include "Channels/MovieSceneDoublePerlinNoiseChannel.h"
#include "MovieSceneDoublePerlinNoiseChannelContainer.generated.h"

/**
 * Double perlin noise channel overriden container
 */
UCLASS(meta=(DisplayName="Double Perlin Noise", ToolTip="Override a channel to use double perlin noise"), MinimalAPI)
class  UMovieSceneDoublePerlinNoiseChannelContainer : public UMovieSceneChannelOverrideContainer
{
	GENERATED_BODY()

public:

	using ChannelType = FMovieSceneDoublePerlinNoiseChannel;

	MOVIESCENETRACKS_API void InitializeOverride(FMovieSceneChannel* InChannel) override;
	MOVIESCENETRACKS_API bool SupportsOverride(FName DefaultChannelTypeName) const override;
	MOVIESCENETRACKS_API void ImportEntityImpl(
			const UE::MovieScene::FChannelOverrideEntityImportParams& OverrideParams, 
			const UE::MovieScene::FEntityImportParams& ImportParams, UE::MovieScene::FImportedEntity* OutImportedEntity) override;

	const FMovieSceneChannel* GetChannel() const override { return &PerlinNoiseChannel; }
	FMovieSceneChannel* GetChannel() override { return &PerlinNoiseChannel; }

#if WITH_EDITOR
	MOVIESCENETRACKS_API FMovieSceneChannelHandle AddChannelProxy(FName ChannelName, FMovieSceneChannelProxyData& ProxyData, const FMovieSceneChannelMetaData& MetaData) override;
#else
	MOVIESCENETRACKS_API void AddChannelProxy(FName ChannelName, FMovieSceneChannelProxyData& ProxyData) override;
#endif

private:

	UPROPERTY(EditAnywhere, Category = "Perlin Noise")
	FMovieSceneDoublePerlinNoiseChannel PerlinNoiseChannel;

	friend class FMovieSceneDoublePerlinNoiseChannelDetailsCustomization;
};

