// Copyright Epic Games, Inc. All Rights Reserved.

#include "Sequencer/LevelSequence/NiagaraSystemSpawnSection.h"
#include "MovieScene/MovieSceneNiagaraSystemSpawnSection.h"
#include "SequencerSectionPainter.h"

FNiagaraSystemSpawnSection::FNiagaraSystemSpawnSection(UMovieSceneSection& InSection)
	: Section(Cast<UMovieSceneNiagaraSystemSpawnSection>(&InSection))
{
}

UMovieSceneSection* FNiagaraSystemSpawnSection::FNiagaraSystemSpawnSection::GetSectionObject()
{
	return Section;
}

int32 FNiagaraSystemSpawnSection::OnPaintSection(FSequencerSectionPainter& InPainter) const
{
	return InPainter.PaintSectionBackground(FLinearColor::Red);
}

FText FNiagaraSystemSpawnSection::GetSectionTitle() const
{
	return NSLOCTEXT("NiagaraSystemSpawnSection", "LifeCycleLabel", "Life Cycle");
}