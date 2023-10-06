// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ISequencer.h"
#include "ISequencerOutlinerColumn.h"

/**
 * Base class for handling creation of outliner column widgets, column settings, and column properties.
 */
class SEQUENCER_API FMovieSceneOutlinerColumn
	: public ISequencerOutlinerColumn
{
public:

	/** Constructor */
	FMovieSceneOutlinerColumn();

	/** Destructor */
	virtual ~FMovieSceneOutlinerColumn();

public:

	virtual bool SupportsSequence(UMovieSceneSequence* InSequence) const override { return true; }

	virtual TSharedRef<SWidget> CreateColumnWidget(UE::Sequencer::TViewModelPtr<UE::Sequencer::IOutlinerExtension> InOutlinerExtension) const = 0;
};
