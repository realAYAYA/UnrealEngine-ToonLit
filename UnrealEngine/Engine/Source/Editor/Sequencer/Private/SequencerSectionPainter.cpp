// Copyright Epic Games, Inc. All Rights Reserved.

#include "SequencerSectionPainter.h"
#include "MovieSceneSection.h"
#include "MovieSceneTrack.h"
#include "MVVM/ViewModels/SectionModel.h"

FSequencerSectionPainter::FSequencerSectionPainter(FSlateWindowElementList& OutDrawElements, const FGeometry& InSectionGeometry, TSharedPtr<UE::Sequencer::FSectionModel> InSection)
	: SectionModel(InSection)
	, DrawElements(OutDrawElements)
	, SectionGeometry(InSectionGeometry)
	, LayerId(0)
	, bParentEnabled(true)
	, bIsHighlighted(false)
	, bIsSelected(false)
	, GhostAlpha(1.f)
{
}

FSequencerSectionPainter::~FSequencerSectionPainter()
{
}

int32 FSequencerSectionPainter::PaintSectionBackground()
{
	FLinearColor TrackColor = FLinearColor(GetTrack()->GetColorTint());
	FLinearColor SectionColor = FLinearColor(SectionModel->GetSection()->GetColorTint());

	const float Alpha = SectionColor.A;
	SectionColor.A = 1.f;

	FLinearColor BackgroundColor = TrackColor * (1.f - Alpha) + SectionColor * Alpha;
	return PaintSectionBackground(BackgroundColor);
}

UMovieSceneTrack* FSequencerSectionPainter::GetTrack() const
{
	return SectionModel->GetSection()->GetTypedOuter<UMovieSceneTrack>();
}

FLinearColor FSequencerSectionPainter::BlendColor(FLinearColor InColor)
{
	static FLinearColor BaseColor(FColor(71,71,71));

	const float Alpha = InColor.A;
	InColor.A = 1.f;
	
	return BaseColor * (1.f - Alpha) + InColor * Alpha;
}
