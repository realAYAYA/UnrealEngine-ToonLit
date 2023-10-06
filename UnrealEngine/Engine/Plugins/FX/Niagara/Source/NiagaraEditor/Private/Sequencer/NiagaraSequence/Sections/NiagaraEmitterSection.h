// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ISequencerSection.h"

class UMovieSceneNiagaraEmitterSection;

/**
*	Visual representation of UMovieSceneNiagaraEmitterTimedSection
*/
class FNiagaraEmitterSection : public ISequencerSection
{
public:
	FNiagaraEmitterSection(UMovieSceneNiagaraEmitterSection &InEmitterSection);

	//~ ISequencerSection interface
	virtual UMovieSceneSection* GetSectionObject(void) override;
	virtual int32 OnPaintSection(FSequencerSectionPainter& InPainter) const override;
	virtual FText GetSectionTitle(void) const override;
	virtual float GetSectionHeight() const override { return 20.0f; }

private:
	UMovieSceneNiagaraEmitterSection* EmitterSection;
};