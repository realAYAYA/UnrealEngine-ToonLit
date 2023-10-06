// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ISequencerSection.h"

class UMovieSceneSection;
class UMovieSceneNiagaraSystemSpawnSection;

class FNiagaraSystemSpawnSection : public ISequencerSection
{
public:
	FNiagaraSystemSpawnSection(UMovieSceneSection& InSection);

	//~ ISequencerSection interface
	virtual UMovieSceneSection* GetSectionObject() override;
	virtual int32 OnPaintSection(FSequencerSectionPainter& InPainter) const override;
	virtual FText GetSectionTitle() const override;

private:
	UMovieSceneNiagaraSystemSpawnSection* Section;
};