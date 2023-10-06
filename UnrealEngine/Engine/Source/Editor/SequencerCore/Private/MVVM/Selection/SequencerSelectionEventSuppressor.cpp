// Copyright Epic Games, Inc. All Rights Reserved.

#include "MVVM/Selection/SequencerSelectionEventSuppressor.h"
#include "MVVM/Selection/SequencerCoreSelection.h"

namespace UE::Sequencer
{

FSelectionEventSuppressor::FSelectionEventSuppressor(FSequencerCoreSelection* InSelection)
	: Selection(InSelection)
{
	if (Selection)
	{
		++Selection->ScopedSuppressionCount;
	}
}

FSelectionEventSuppressor::~FSelectionEventSuppressor()
{
	if (Selection)
	{
		const int32 NewCount = --Selection->ScopedSuppressionCount;
		ensureMsgf(NewCount >= 0, TEXT("RAII Mismatch between scoped selection suppressors! Count reached %d"), NewCount);

		if (NewCount == 0)
		{
			Selection->BroadcastSelectionChanged();
		}
	}
}


} // namespace UE::Sequencer