// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneTextChannel.h"
#include "Channels/MovieSceneChannelHandle.h"
#include "Widgets/SCompoundWidget.h"

class ISequencer;

namespace UE::MovieScene
{

struct FTextKeyEditorParams
{
	FGuid ObjectBindingID;
	TMovieSceneChannelHandle<FMovieSceneTextChannel> ChannelHandle;
	TWeakObjectPtr<UMovieSceneSection> WeakSection;
	TWeakPtr<ISequencer> WeakSequencer;
	TWeakPtr<FTrackInstancePropertyBindings> WeakPropertyBindings;
	TFunction<TOptional<FText>(UObject&, FTrackInstancePropertyBindings*)> OnGetExternalValue;
};

class STextKeyEditor : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(STextKeyEditor){}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, FTextKeyEditorParams&& InParams);

private:
	/** Gets the evaluated Text value at the current sequencer time */
	FText GetText() const;

	/** Updates the CachedText result to the committed text and adds/updates a key to the text channel at the current sequencer time */
	void OnTextCommitted(const FText& InText, ETextCommit::Type InCommitType);

	FTextKeyEditorParams Params;
};

}
