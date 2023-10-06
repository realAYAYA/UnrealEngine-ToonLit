// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MVVM/ViewModelPtr.h"
#include "Templates/SubclassOf.h"

class UMovieSceneSequence;

namespace UE
{
namespace Sequencer
{
class IOutlinerExtension;
}
}

/**
 * Interface for sequencer outliner columns.
 */
class ISequencerOutlinerColumn
{
	
public:

	/* Gets the unique name of the column to use in the SOutlinerView registry and context menus */
	virtual FName GetColumnName() const = 0;

	/* The default visibility state of this column when loaded for the first time */
	virtual bool IsColumnVisibleByDefault() const { return true; }

	/* Gets whether or not this column is supported by a given Sequencer */
	virtual bool SupportsSequence(UMovieSceneSequence* InSequence) const = 0;

	/* Gets the widget created for each item within the SOutlinerView, column widgets must be fixed width */
	virtual TSharedRef<SWidget> CreateColumnWidget(UE::Sequencer::TViewModelPtr<UE::Sequencer::IOutlinerExtension>InOutlinerExtension) const = 0;

public:

	/** Virtual destructor. */
	virtual ~ISequencerOutlinerColumn() { }
	
};
