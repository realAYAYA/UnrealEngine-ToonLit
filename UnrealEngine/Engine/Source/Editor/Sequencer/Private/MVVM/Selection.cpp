// Copyright Epic Games, Inc. All Rights Reserved.

#include "MVVM/Selection/Selection.h"
#include "MVVM/SharedViewModelData.h"
#include "MVVM/ViewModels/EditorViewModel.h"
#include "MVVM/ViewModels/SectionModel.h"
#include "MVVM/ViewModels/ChannelModel.h"
#include "MVVM/Extensions/IObjectBindingExtension.h"
#include "MVVM/Extensions/ITrackExtension.h"
#include "MVVM/Extensions/ISelectableExtension.h"

namespace UE::Sequencer
{

bool FTrackAreaSelection::OnSelectItem(const FWeakViewModelPtr& WeakViewModel)
{
	if (TSharedPtr<FViewModel> ViewModel = WeakViewModel.Pin())
	{
		ISelectableExtension* Selectable = ViewModel->CastThis<ISelectableExtension>();
		if (Selectable && Selectable->IsSelectable() == ESelectionIntent::Never)
		{
			return false;
		}

		return true;
	}

	return false;
}

FSequencerSelection::FSequencerSelection()
{
	AddSelectionSet(&Outliner);
	AddSelectionSet(&TrackArea);
	AddSelectionSet(&KeySelection);
	AddSelectionSet(&MarkedFrames);
}

void FSequencerSelection::Initialize(TViewModelPtr<FEditorViewModel> InViewModel)
{
	FViewModelPtr RootModel = InViewModel->GetRootModel();
	if (RootModel)
	{
		FSimpleMulticastDelegate& HierarchyChanged = RootModel->GetSharedData()->SubscribeToHierarchyChanged(RootModel);
		HierarchyChanged.AddSP(this, &FSequencerSelection::OnHierarchyChanged);
	}
}

void FSequencerSelection::Empty()
{
	FSelectionEventSuppressor EventSuppressor = SuppressEvents();

	Outliner.Empty();
	TrackArea.Empty();
	KeySelection.Empty();
	MarkedFrames.Empty();
}

void FSequencerSelection::PreSelectionSetChangeEvent(FSelectionBase* InSelectionSet)
{
	if (InSelectionSet == &Outliner)
	{
		// Empty the track area selection when selecting anything on the outliner
		if (!TrackArea.HasPendingChanges() && !KeySelection.HasPendingChanges())
		{
			TrackArea.Empty();
			KeySelection.Empty();
		}
	}
}

void FSequencerSelection::PreBroadcastChangeEvent()
{
	// Repopulate the nodes with keys or sections set

	// First off reset the selection states from the previous set
	for (TWeakViewModelPtr<IOutlinerExtension> WeakOldNode : NodesWithKeysOrSections)
	{
		TViewModelPtr<IOutlinerExtension> OldNode = WeakOldNode.Pin();
		if (OldNode)
		{
			OldNode->ToggleSelectionState(EOutlinerSelectionState::HasSelectedKeys | EOutlinerSelectionState::HasSelectedTrackAreaItems, false);

			OldNode = OldNode.AsModel()->FindAncestorOfType<IOutlinerExtension>();
			while (OldNode)
			{
				OldNode->ToggleSelectionState(EOutlinerSelectionState::DescendentHasSelectedTrackAreaItems | EOutlinerSelectionState::DescendentHasSelectedKeys, false);
				OldNode = OldNode.AsModel()->FindAncestorOfType<IOutlinerExtension>();
			}
		}
	}

	// Reset the selection set
	NodesWithKeysOrSections.Reset();

	// Gather selection states from selected track area items
	for (FViewModelPtr TrackAreaModel : TrackArea)
	{
		TViewModelPtr<IOutlinerExtension> ParentOutlinerNode = TrackAreaModel->FindAncestorOfType<IOutlinerExtension>();
		if (ParentOutlinerNode)
		{
			ParentOutlinerNode->ToggleSelectionState(EOutlinerSelectionState::HasSelectedTrackAreaItems, true);
			NodesWithKeysOrSections.Add(ParentOutlinerNode);
		}

		ParentOutlinerNode = ParentOutlinerNode.AsModel()->FindAncestorOfType<IOutlinerExtension>();
		while (ParentOutlinerNode)
		{
			ParentOutlinerNode->ToggleSelectionState(EOutlinerSelectionState::DescendentHasSelectedTrackAreaItems, true);
			ParentOutlinerNode = ParentOutlinerNode.AsModel()->FindAncestorOfType<IOutlinerExtension>();
		}
	}

	// Gather selection states from selected keys
	for (FKeyHandle Key : KeySelection)
	{
		TViewModelPtr<FChannelModel>      Channel            = KeySelection.GetModelForKey(Key);
		TViewModelPtr<IOutlinerExtension> ParentOutlinerNode = Channel ? Channel->GetLinkedOutlinerItem() : nullptr;
		if (ParentOutlinerNode)
		{
			ParentOutlinerNode->ToggleSelectionState(EOutlinerSelectionState::HasSelectedKeys, true);
			NodesWithKeysOrSections.Add(ParentOutlinerNode);
		}

		ParentOutlinerNode = ParentOutlinerNode.AsModel()->FindAncestorOfType<IOutlinerExtension>();
		while (ParentOutlinerNode)
		{
			ParentOutlinerNode->ToggleSelectionState(EOutlinerSelectionState::DescendentHasSelectedKeys, true);
			ParentOutlinerNode = ParentOutlinerNode.AsModel()->FindAncestorOfType<IOutlinerExtension>();
		}
	}

	FSelectionEventSuppressor EventSuppressor = SuppressEvents();
	FOutlinerSelection OutlinerCopy = Outliner;

	// Select any outliner nodes that don't have keys or sections selected
	for (TViewModelPtr<IOutlinerExtension> OutlinerItem : Outliner)
	{
		bool bFound = false;
		bool bAnyIndirectSelection = false;

		TSharedPtr<IObjectBindingExtension> ObjectBindingItem = OutlinerItem.AsModel()->FindAncestorOfType<IObjectBindingExtension>(true);

		for (TViewModelPtr<IOutlinerExtension> IndirectItem : IterateIndirectOutlinerSelection())
		{
			bAnyIndirectSelection = true;

			if (IndirectItem == OutlinerItem)
			{
				bFound = true;
				break;
			}

			for (TSharedPtr<IObjectBindingExtension> IndirectObjectBindingItem : IndirectItem.AsModel()->GetAncestorsOfType<IObjectBindingExtension>(true))
			{
				if (IndirectObjectBindingItem == ObjectBindingItem)
				{
					bFound = true;
					break;
				}
			}

			if (bFound)
			{
				break;
			}
		}

		if (bAnyIndirectSelection && !bFound)
		{
			OutlinerCopy.Deselect(OutlinerItem);
		}
	}

	Outliner = OutlinerCopy;
}

FIndirectOutlinerSelectionIterator FSequencerSelection::IterateIndirectOutlinerSelection() const
{
	return FIndirectOutlinerSelectionIterator{ &NodesWithKeysOrSections };
}

TArray<FGuid> FSequencerSelection::GetBoundObjectsGuids()
{
	TArray<FGuid> OutGuids;

	for (const TWeakViewModelPtr<IOutlinerExtension>& WeakModel : NodesWithKeysOrSections)
	{
		FViewModelPtr Model = WeakModel.Pin();
		if (Model)
		{
			TSharedPtr<IObjectBindingExtension> ObjectBinding = Model->FindAncestorOfType<IObjectBindingExtension>(true);
			if (ObjectBinding)
			{
				OutGuids.Add(ObjectBinding->GetObjectGuid());
			}
		}
	}

	if (OutGuids.Num() == 0)
	{
		for (FViewModelPtr Model : Outliner)
		{
			TSharedPtr<IObjectBindingExtension> ObjectBinding = Model->FindAncestorOfType<IObjectBindingExtension>(true);
			if (ObjectBinding)
			{
				OutGuids.Add(ObjectBinding->GetObjectGuid());
			}
		}
	}

	return OutGuids;
}

TSet<UMovieSceneSection*> FSequencerSelection::GetSelectedSections() const
{
	TSet<UMovieSceneSection*> SelectedSections;
	SelectedSections.Reserve(TrackArea.Num());

	for (TViewModelPtr<FSectionModel> Model : TrackArea.Filter<FSectionModel>())
	{
		if (UMovieSceneSection* Section = Model->GetSection())
		{
			SelectedSections.Add(Section);
		}
	}

	return SelectedSections;
}

TSet<UMovieSceneTrack*> FSequencerSelection::GetSelectedTracks() const
{
	TSet<UMovieSceneTrack*> SelectedTracks;
	SelectedTracks.Reserve(TrackArea.Num());

	for (TViewModelPtr<ITrackExtension> TrackExtension : Outliner.Filter<ITrackExtension>())
	{
		if (UMovieSceneTrack* Track = TrackExtension->GetTrack())
		{
			SelectedTracks.Add(Track);
		}
	}

	return SelectedTracks;
}

void FSequencerSelection::OnHierarchyChanged()
{
	// This is an esoteric hack that ensures we re-synchronize external (ie Actor)
	// selection when models are removed from the tree. Doing so ensures that
	// FSequencer::SynchronizeExternalSelectionWithSequencerSelection is called within
	// the scope of GIsTransacting being true, which prevents that function from creating new
	// transactions for the selection synchronization. This is important because otherwise
	// the undo/redo stack gets wiped by actor selections when undoing if the selection is
	// not identical
	RevalidateSelection();
}

void FSequencerSelection::RevalidateSelection()
{
	FSelectionEventSuppressor EventSuppressor = SuppressEvents();

	KeySelection.RemoveByPredicate(
		[this](FKeyHandle Key)
		{
			TViewModelPtr<FChannelModel> Channel = this->KeySelection.GetModelForKey(Key);
			return !Channel || Channel->GetSection() == nullptr;
		}
	);

	TrackArea.RemoveByPredicate(
		[this](const FWeakViewModelPtr& Key)
		{
			return Key.Pin() == nullptr;
		}
	);

	Outliner.RemoveByPredicate(
		[this](const TWeakViewModelPtr<IOutlinerExtension>& Key)
		{
			return Key.Pin() == nullptr;
		}
	);
}

} // namespace UE::Sequencer