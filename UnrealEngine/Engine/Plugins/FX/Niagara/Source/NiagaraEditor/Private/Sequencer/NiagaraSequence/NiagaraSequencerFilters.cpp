// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraSequencerFilters.h"
#include "MovieScene/MovieSceneNiagaraTrack.h"
#include "NiagaraActor.h"
#include "NiagaraComponent.h"
#include "Styling/AppStyle.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NiagaraSequencerFilters)

#define LOCTEXT_NAMESPACE "NiagaraSequencerTrackFilters"

//////////////////////////////////////////////////////////////////////////
//

class FSequencerTrackFilter_NiagaraTracks : public FSequencerTrackFilter
{
	virtual FString GetName() const override { return TEXT("SequencerNiagaraTrackFilter"); }
	virtual FText GetDisplayName() const override { return LOCTEXT("SequencerTrackFilter_NiagaraTracks", "Niagara"); }
	virtual FText GetToolTipText() const override { return LOCTEXT("SequencerTrackFilter_NiagaraTracksToolTip", "Show only Niagara tracks."); }
	virtual FSlateIcon GetIcon() const { return FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.ParticleSystem"); }

	// IFilter implementation
	virtual bool PassesFilter(FTrackFilterType InItem) const override
	{
		if (!InItem)
		{
			return false;
		}

		if (InItem->IsA(UMovieSceneNiagaraTrack::StaticClass()) || InItem->IsA(ANiagaraActor::StaticClass()) || InItem->IsA(UNiagaraComponent::StaticClass()))
		{
			return true;
		}

		const AActor* Actor = Cast<const AActor>(InItem);
		if (Actor)
		{
			if (Actor->FindComponentByClass(UNiagaraComponent::StaticClass()))
			{
				return true;
			}
		}

		return false;
	}
};

//////////////////////////////////////////////////////////////////////////
//

void UNiagaraSequencerTrackFilter::AddTrackFilterExtensions(TArray< TSharedRef<class FSequencerTrackFilter> >& InOutFilterList) const
{
	InOutFilterList.Add(MakeShared<FSequencerTrackFilter_NiagaraTracks>());
}

#undef LOCTEXT_NAMESPACE

