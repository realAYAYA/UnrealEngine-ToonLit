// Copyright Epic Games, Inc. All Rights Reserved.

#include "LiveLinkSequencerFilters.h"
#include "MovieScene/MovieSceneLiveLinkTrack.h"
#include "LiveLinkComponent.h"
#include "Styling/SlateIconFinder.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LiveLinkSequencerFilters)

#define LOCTEXT_NAMESPACE "LiveLinkSequencerTrackFilters"

//////////////////////////////////////////////////////////////////////////
//

class FSequencerTrackFilter_LiveLinkTracks : public FSequencerTrackFilter_ClassType<UMovieSceneLiveLinkTrack>
{
	virtual FString GetName() const override { return TEXT("SequencerLiveLinkTrackFilter"); }
	virtual FText GetDisplayName() const override { return LOCTEXT("SequencerTrackFilter_LiveLinkTracks", "Live Link"); }
	virtual FText GetToolTipText() const override { return LOCTEXT("SequencerTrackFilter_LiveLinkTracksToolTip", "Show only Live Link tracks."); }
	virtual FSlateIcon GetIcon() const { return FSlateIconFinder::FindIconForClass(ULiveLinkComponent::StaticClass()); }
	virtual bool SupportsSequence(UMovieSceneSequence* InSequence) const override
	{
		if (InSequence && InSequence->IsTrackSupported(UMovieSceneLiveLinkTrack::StaticClass()) == ETrackSupport::NotSupported)
		{
			return false;
		}

		return FSequencerTrackFilter_ClassType<UMovieSceneLiveLinkTrack>::SupportsSequence(InSequence);
	}
};

//////////////////////////////////////////////////////////////////////////
//

void ULiveLinkSequencerTrackFilter::AddTrackFilterExtensions(TArray< TSharedRef<class FSequencerTrackFilter> >& InOutFilterList) const
{
	InOutFilterList.Add(MakeShared<FSequencerTrackFilter_LiveLinkTracks>());
}

#undef LOCTEXT_NAMESPACE

