// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaSequencePlaybackDetails.h"
#include "AvaSequencePlaybackObject.h"
#include "AvaSequence.h"
#include "AvaSequencer.h"
#include "CustomDetailsViewArgs.h"
#include "CustomDetailsViewModule.h"

#define LOCTEXT_NAMESPACE "AvaSequencePlaybackDetails"

FName FAvaSequencePlaybackDetails::GetSectionName() const
{
	return TEXT("Playback");
}

FText FAvaSequencePlaybackDetails::GetSectionDisplayName() const
{
	return LOCTEXT("PlaybackLabel", "Playback");
}

TSharedRef<SWidget> FAvaSequencePlaybackDetails::CreateContentWidget(const TSharedRef<FAvaSequencer>& InAvaSequencer)
{
	AvaSequencerWeak = InAvaSequencer;

	FCustomDetailsViewArgs CustomDetailsViewArgs;
	CustomDetailsViewArgs.IndentAmount           = 0.f;
	CustomDetailsViewArgs.ValueColumnWidth       = 0.5f;
	CustomDetailsViewArgs.bShowCategories        = true;
	CustomDetailsViewArgs.bAllowGlobalExtensions = true;
	CustomDetailsViewArgs.CategoryAllowList.Allow(TEXT("Scheduled Playback"));
	CustomDetailsViewArgs.ExpansionState.Add(FCustomDetailsViewItemId::MakeCategoryId("Scheduled Playback"), true);

	TSharedRef<ICustomDetailsView> PlaybackDetailsView = ICustomDetailsViewModule::Get().CreateCustomDetailsView(CustomDetailsViewArgs);

	IAvaSequencePlaybackObject* PlaybackObject = InAvaSequencer->GetProvider().GetPlaybackObject();
	if (ensureAlways(PlaybackObject))
	{
		PlaybackDetailsView->SetObject(PlaybackObject->ToUObject());
	}

	return PlaybackDetailsView;
}

bool FAvaSequencePlaybackDetails::ShouldShowSection() const
{
	TSharedPtr<FAvaSequencer> AvaSequencer = AvaSequencerWeak.Pin();
	return AvaSequencer.IsValid() && IsValid(AvaSequencer->GetViewedSequence());
}

#undef LOCTEXT_NAMESPACE
