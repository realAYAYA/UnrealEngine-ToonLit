// Copyright Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTextSection.h"
#include "TextComponentTypes.h"
#include "Tracks/MovieScenePropertyTrack.h"

UMovieSceneTextSection::UMovieSceneTextSection(const FObjectInitializer& InObjectInitializer)
	: Super(InObjectInitializer)
{
	bSupportsInfiniteRange = true;
	EvalOptions.EnableAndSetCompletionMode(EMovieSceneCompletionMode::ProjectDefault);

#if WITH_EDITOR
	ChannelProxy = MakeShared<FMovieSceneChannelProxy>(TextChannel, FMovieSceneChannelMetaData(), TMovieSceneExternalValue<FText>::Make());
	TextChannel.SetPackage(GetPackage());
#else
	ChannelProxy = MakeShared<FMovieSceneChannelProxy>(TextChannel);
#endif
}

bool UMovieSceneTextSection::PopulateEvaluationFieldImpl(const TRange<FFrameNumber>& EffectiveRange
	, const FMovieSceneEvaluationFieldEntityMetaData& InMetaData
	, FMovieSceneEntityComponentFieldBuilder* OutFieldBuilder)
{
	FMovieScenePropertyTrackEntityImportHelper::PopulateEvaluationField(*this, EffectiveRange, InMetaData, OutFieldBuilder);
	return true;
}

void UMovieSceneTextSection::ImportEntityImpl(UMovieSceneEntitySystemLinker* EntityLinker
	, const FEntityImportParams& Params
	, FImportedEntity* OutImportedEntity)
{
	using namespace UE::MovieScene;

	if (!TextChannel.HasAnyData())
	{
		return;
	}

	const FTextComponentTypes* TextComponents = FTextComponentTypes::Get();
	FPropertyTrackEntityImportHelper(TextComponents->Text)
		.Add(TextComponents->TextChannel, &TextChannel)
		.Commit(this, Params, OutImportedEntity);
}
