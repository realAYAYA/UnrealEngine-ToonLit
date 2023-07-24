// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraEmitterSection.h"
#include "MovieSceneNiagaraEmitterSection.h"
#include "ViewModels/NiagaraEmitterHandleViewModel.h"
#include "ViewModels/NiagaraEmitterViewModel.h"

#include "SequencerSectionPainter.h"
#include "ISectionLayoutBuilder.h"

#define LOCTEXT_NAMESPACE "NiagaraEmitterTimedSection"

FNiagaraEmitterSection::FNiagaraEmitterSection(UMovieSceneNiagaraEmitterSection &InEmitterSection)
{
	EmitterSection = &InEmitterSection;
}

UMovieSceneSection* FNiagaraEmitterSection::GetSectionObject(void)
{
	return EmitterSection;
}

int32 FNiagaraEmitterSection::OnPaintSection(FSequencerSectionPainter& InPainter) const
{
	return InPainter.PaintSectionBackground();

	// TODO: Fix the looping drawing and interaction
	TSharedPtr<FNiagaraEmitterHandleViewModel> EmitterHandleViewModel = EmitterSection->GetEmitterHandleViewModel();
	if (EmitterHandleViewModel.IsValid() == false)
	{
		return InPainter.LayerId;
	}

	/*
	// draw the first run of the emitter
	FSlateDrawElement::MakeBox
	(
		InPainter.DrawElements,
		InPainter.LayerId + 1,
		InPainter.SectionGeometry.ToPaintGeometry(),
		FAppStyle::GetBrush("CurveEd.TimelineArea"),
		ESlateDrawEffect::None,
		FLinearColor(0.3f, 0.3f, 0.6f)
	);

	// draw all loops of the emitter as 'ghosts' of the original section
	float X = InPainter.SectionGeometry.AbsolutePosition.X;
	float GeomW = InPainter.SectionGeometry.GetLocalSize().X;
	int32 NumLoops = EmitterSection->GetNumLoops();
	for (int32 Loop = 0; Loop < NumLoops; Loop++)
	{
		FSlateDrawElement::MakeBox
		(
			InPainter.DrawElements,
			InPainter.LayerId + 1,
			InPainter.SectionGeometry.ToOffsetPaintGeometry(FVector2D(GeomW*(Loop + 1), 0.0f)),
			FAppStyle::GetBrush("CurveEd.TimelineArea"),
			ESlateDrawEffect::None,
			FLinearColor(0.3f, 0.3f, 0.6f, 0.25f)
		);
	}
	*/
	return InPainter.LayerId;
}

FText FNiagaraEmitterSection::GetSectionTitle(void) const
{
	TSharedPtr<FNiagaraEmitterHandleViewModel> EmitterHandleviewModel = EmitterSection->GetEmitterHandleViewModel();
	if (EmitterHandleviewModel.IsValid())
	{
		return EmitterHandleviewModel->GetNameText();
	}
	return FText();
}

#undef LOCTEXT_NAMESPACE
