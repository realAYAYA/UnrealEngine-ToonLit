// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Channels/MovieSceneChannelOverrideContainer.h"
#include "Channels/MovieSceneFloatPerlinNoiseChannel.h"
#include "EntitySystem/IMovieSceneEntityProvider.h"
#include "MovieSceneFloatPerlinNoiseChannelContainer.generated.h"

/**
 * Float perlin noise channel overriden container
 */
UCLASS(meta=(DisplayName="Float Perlin Noise", ToolTip="Override a channel to use float perlin noise"))
class MOVIESCENETRACKS_API UMovieSceneFloatPerlinNoiseChannelContainer : public UMovieSceneChannelOverrideContainer
{
	GENERATED_BODY()

public:

	using ChannelType = FMovieSceneFloatPerlinNoiseChannel;

	void InitializeOverride(FMovieSceneChannel* InChannel) override;
	bool SupportsOverride(FName DefaultChannelTypeName) const override;
	void ImportEntityImpl(
			const UE::MovieScene::FChannelOverrideEntityImportParams& OverrideParams, 
			const UE::MovieScene::FEntityImportParams& ImportParams, UE::MovieScene::FImportedEntity* OutImportedEntity) override;

	const FMovieSceneChannel* GetChannel() const override { return &PerlinNoiseChannel; }
	FMovieSceneChannel* GetChannel() override { return &PerlinNoiseChannel; }

#if WITH_EDITOR
	FMovieSceneChannelHandle AddChannelProxy(FName ChannelName, FMovieSceneChannelProxyData& ProxyData, const FMovieSceneChannelMetaData& MetaData) override;
#else
	void AddChannelProxy(FName ChannelName, FMovieSceneChannelProxyData& ProxyData) override;
#endif

private:

	UPROPERTY(EditAnywhere, Category = "Perlin Noise")
	FMovieSceneFloatPerlinNoiseChannel PerlinNoiseChannel;

	friend class FMovieSceneFloatPerlinNoiseChannelDetailsCustomization;
};

