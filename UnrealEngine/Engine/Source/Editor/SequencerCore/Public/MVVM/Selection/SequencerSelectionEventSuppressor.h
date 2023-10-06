// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"

namespace UE::Sequencer
{

class FSequencerCoreSelection;

/**
 * Scoped object type that temporarily suppresses broadcast of selection events
 * @see FSequencerCoreSelection::SuppressEvents
 */
struct FSelectionEventSuppressor
{
	SEQUENCERCORE_API FSelectionEventSuppressor(FSequencerCoreSelection* InSelection);
	SEQUENCERCORE_API ~FSelectionEventSuppressor();

	FSelectionEventSuppressor(const FSelectionEventSuppressor&) = delete;
	void operator=(const FSelectionEventSuppressor&) = delete;

	FSelectionEventSuppressor(FSelectionEventSuppressor&&) = delete;
	void operator=(FSelectionEventSuppressor&&) = delete;

private:
	FSequencerCoreSelection* Selection;
};


} // namespace UE::Sequencer