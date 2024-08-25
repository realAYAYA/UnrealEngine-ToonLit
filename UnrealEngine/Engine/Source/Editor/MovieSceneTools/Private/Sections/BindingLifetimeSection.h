// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ISequencerSection.h"
#include "ISequencer.h"

class FSequencerSectionPainter;

class FBindingLifetimeSection
	: public FSequencerSection
{
public:

	FBindingLifetimeSection(UMovieSceneSection& InSectionObject, TWeakPtr<ISequencer> InSequencer)
		: FSequencerSection(InSectionObject)
		, Sequencer(InSequencer)
	{}

protected:

	bool IsSectionSelected() const; 
	virtual int32 OnPaintSection(FSequencerSectionPainter& Painter) const override;

protected:

	TWeakPtr<ISequencer> Sequencer;
};
