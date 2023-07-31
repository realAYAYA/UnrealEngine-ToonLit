// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Tree/CurveEditorTreeFilter.h"

/**
 * A specialized filter for showing items in the curve editor selected from the sequencer panel.
 * This filter will store the selected nodes and all the parents of the selected nodes in a NodesToFilter set.
 * An item will pass if it is either directly selected or has a parent in the set.
 */
struct FSequencerSelectionCurveFilter : FCurveEditorTreeFilter
{
	static const int32 FilterPass = -1000;

	FSequencerSelectionCurveFilter()
		: FCurveEditorTreeFilter(ISequencerModule::GetSequencerSelectionFilterType(), FilterPass)
	{}

	/**
	 * Adds all selected nodes and their object parents to the NodesToFilter set
	 */
	void Update(const TSet<TWeakPtr<UE::Sequencer::FViewModel>>& SelectedNodes, const bool bExpandTreeToSelectedNodes = true)
	{
		using namespace UE::Sequencer;

		NodesToFilter.Empty(SelectedNodes.Num());

		for (const TWeakPtr<FViewModel>& WeakSelectedNode : SelectedNodes)
		{
			if (TSharedPtr<FViewModel> SelectedNode = WeakSelectedNode.Pin())
			{
				NodesToFilter.Add(WeakSelectedNode);

				TSharedPtr<FViewModel> Parent = SelectedNode->GetParent();
				while (Parent.IsValid())
				{
					if (Parent->IsA<IObjectBindingExtension>())
					{
						NodesToFilter.Add(Parent);
						break;
					}

					Parent = Parent->GetParent();
				}
			}
		}

		bExpandToMatchedItems = bExpandTreeToSelectedNodes;
	}

	bool Match(TSharedRef<const UE::Sequencer::FViewModel> InNode) const
	{
		return NodesToFilter.Contains(InNode);
	}

private:

	TSet<TWeakPtr<const UE::Sequencer::FViewModel>> NodesToFilter;
};

