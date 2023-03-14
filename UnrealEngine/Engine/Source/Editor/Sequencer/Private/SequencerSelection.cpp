// Copyright Epic Games, Inc. All Rights Reserved.

#include "SequencerSelection.h"

#include "Containers/SparseArray.h"
#include "MVVM/Extensions/IObjectBindingExtension.h"
#include "MVVM/Extensions/IOutlinerExtension.h"
#include "MVVM/Extensions/ISelectableExtension.h"
#include "MVVM/SectionModelStorageExtension.h"
#include "MVVM/SharedViewModelData.h"
#include "MVVM/ViewModelTypeID.h"
#include "MVVM/ViewModels/ChannelModel.h"
#include "MVVM/ViewModels/SectionModel.h"
#include "MVVM/ViewModels/TrackModel.h"
#include "MVVM/ViewModels/ViewModelIterators.h"
#include "Misc/AssertionMacros.h"
#include "Misc/Guid.h"
#include "MovieSceneSection.h"
#include "MovieSceneTrack.h"
#include "SequencerCoreFwd.h"
#include "Templates/ChooseClass.h"
#include "Templates/TypeHash.h"
#include "UObject/WeakObjectPtr.h"

FSequencerSelection::FSequencerSelection()
	: SerialNumber(0)
	, SuspendBroadcastCount(0)
	, bOutlinerNodeSelectionChangedBroadcastPending(false)
	, bEmptySelectedOutlinerNodesWithSectionsPending(false)
	, bNodesWithSelectedKeysOrSectionsDirty(false)
{
}

TSharedPtr<UE::Sequencer::FViewModel> FSequencerSelection::GetRootModel() const
{
	return RootModel;
}

void FSequencerSelection::SetRootModel(TSharedPtr<UE::Sequencer::FViewModel> InRootModel)
{
	if (RootModel && HierarchyChangedHandle.IsValid())
	{
		RootModel->GetSharedData()->UnsubscribeFromHierarchyChanged(RootModel, HierarchyChangedHandle);
		HierarchyChangedHandle = FDelegateHandle();
	}

	RootModel = InRootModel;

	if (RootModel)
	{
		FSimpleMulticastDelegate& HierarchyChanged = RootModel->GetSharedData()->SubscribeToHierarchyChanged(RootModel);
		HierarchyChangedHandle = HierarchyChanged.AddRaw(this, &FSequencerSelection::OnHierarchyChanged);
	}
}

const TSet<FSequencerSelectedKey>& FSequencerSelection::GetSelectedKeys() const
{
	return SelectedKeys;
}

const TSet<FKeyHandle>& FSequencerSelection::GetRawSelectedKeys() const
{
	return RawSelectedKeys;
}

TSet<TWeakObjectPtr<UMovieSceneSection>> FSequencerSelection::GetSelectedSections() const
{
	using namespace UE::Sequencer;

	TSet<TWeakObjectPtr<UMovieSceneSection>> Sections;

	for (const TWeakPtr<FViewModel>& TrackAreaModel : SelectedTrackAreaItems)
	{
		if (TSharedPtr<FViewModel> Pinned = TrackAreaModel.Pin())
		{
			FSectionModel* Section = Pinned->CastThis<FSectionModel>();
			if (Section && Section->GetSection())
			{
				Sections.Add(Section->GetSection());
			}
		}
	}

	return Sections;
}

const TSet<TWeakPtr<UE::Sequencer::FViewModel>>& FSequencerSelection::GetSelectedOutlinerItems() const
{
	return SelectedOutlinerItems;
}

const TSet<TWeakPtr<UE::Sequencer::FViewModel>>& FSequencerSelection::GetSelectedTrackAreaItems() const
{
	return SelectedTrackAreaItems;
}

const TSet<TWeakPtr<UE::Sequencer::FViewModel>>& FSequencerSelection::GetNodesWithSelectedKeysOrSections() const
{
	using namespace UE::Sequencer;

	if (!bNodesWithSelectedKeysOrSectionsDirty)
	{
		return NodesWithSelectedKeysOrSections;
	}

	bNodesWithSelectedKeysOrSectionsDirty = false;
	NodesWithSelectedKeysOrSections.Empty();

	// Handle selected keys
	for (const FSequencerSelectedKey& Key : SelectedKeys)
	{
		if (TSharedPtr<FChannelModel> Channel = Key.WeakChannel.Pin())
		{
			TViewModelPtr<IOutlinerExtension> Outliner = Channel->GetLinkedOutlinerItem();
			if (Outliner)
			{
				NodesWithSelectedKeysOrSections.Add(Outliner.AsModel());
			}
		}
	}

	// Handle selected track area items
	for (TWeakPtr<FViewModel> WeakTrackAreaModel : SelectedTrackAreaItems)
	{
		TSharedPtr<FViewModel> TrackAreaModel = WeakTrackAreaModel.Pin();
		if (TrackAreaModel && TrackAreaModel->IsA<FSectionModel>())
		{
			TViewModelPtr<IOutlinerExtension> OutlinerItem = TrackAreaModel->FindAncestorOfType<IOutlinerExtension>();
			if (OutlinerItem)
			{
				NodesWithSelectedKeysOrSections.Add(OutlinerItem.AsModel());
			}
		}
	}

	return NodesWithSelectedKeysOrSections;
}

FSequencerSelection::FOnSelectionChanged& FSequencerSelection::GetOnKeySelectionChanged()
{
	return OnKeySelectionChanged;
}

FSequencerSelection::FOnSelectionChanged& FSequencerSelection::GetOnOutlinerNodeSelectionChanged()
{
	return OnOutlinerNodeSelectionChanged;
}

FSequencerSelection::FOnSelectionChangedObjectGuids& FSequencerSelection::GetOnOutlinerNodeSelectionChangedObjectGuids()
{
	return OnOutlinerNodeSelectionChangedObjectGuids;
}

TArray<FGuid> FSequencerSelection::GetBoundObjectsGuids()
{
	using namespace UE::Sequencer;

	TArray<FGuid> OutGuids;
	TSet<TWeakPtr<FViewModel>> SelectedItems = GetNodesWithSelectedKeysOrSections();
	if (SelectedItems.Num() == 0)
	{
		SelectedItems = GetSelectedOutlinerItems();
	}
	for (TWeakPtr<FViewModel> WeakModel : SelectedItems)
	{
		TSharedPtr<FViewModel> Model = WeakModel.Pin();
		if (Model)
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

TArray<UMovieSceneTrack*> FSequencerSelection::GetSelectedTracks() const
{
	using namespace UE::Sequencer;

	TArray<UMovieSceneTrack*> OutTracks;

	for (TWeakPtr<FViewModel> WeakModel : SelectedOutlinerItems)
	{
		TSharedPtr<FViewModel> Model = WeakModel.Pin();
		if (Model)
		{
			TSharedPtr<FTrackModel> Track = Model->FindAncestorOfType<FTrackModel>(true);
			if (Track)
			{
				OutTracks.Add(Track->GetTrack());
			}
		}
	}

	return OutTracks;
}

void FSequencerSelection::AddToSelection(TSharedPtr<UE::Sequencer::FViewModel> InModel)
{
	if (!ensureAlwaysMsgf(InModel, TEXT("AddToSelection with null view model")))
	{
		return;
	}

	if (InModel->IsA<UE::Sequencer::IOutlinerExtension>())
	{
		AddToOutlinerSelection(InModel);
	}
	else
	{
		AddToTrackAreaSelection(InModel);
	}
}

void FSequencerSelection::AddToTrackAreaSelection(TSharedPtr<UE::Sequencer::FViewModel> InModel)
{
	if (!ensureAlwaysMsgf(InModel, TEXT("AddToSelection with null view model")))
	{
		return;
	}

	using namespace UE::Sequencer;

	ISelectableExtension* Selectable = InModel->CastThis<ISelectableExtension>();
	if (Selectable && Selectable->IsSelectable() == ESelectionIntent::Never)
	{
		return;
	}

	bNodesWithSelectedKeysOrSectionsDirty = true;
	++SerialNumber;
	SelectedTrackAreaItems.Add(InModel);

	if (TSharedPtr<IOutlinerExtension> ParentItem = InModel->FindAncestorOfType<IOutlinerExtension>())
	{
		ParentItem->ToggleSelectionState(EOutlinerSelectionState::HasSelectedTrackAreaItems, true);
	}

	if (FSectionModel* SectionModel = InModel->CastThis<FSectionModel>())
	{
		if (UMovieSceneSection* Section = SectionModel->GetSection())
		{
			if ( IsBroadcasting() )
			{
				OnSectionSelectionChanged.Broadcast();
				OnOutlinerNodeSelectionChangedObjectGuids.Broadcast();
			}

			// Deselect any outliner nodes that aren't within the trunk of this section
			EmptySelectedOutlinerNodesWithoutSections(TArray<UMovieSceneSection*>{Section});
		}
	}
}

void FSequencerSelection::AddToSelection(const FSequencerSelectedKey& Key)
{
	using namespace UE::Sequencer;

	bNodesWithSelectedKeysOrSectionsDirty = true;
	++SerialNumber;

	SelectedKeys.Add(Key);
	RawSelectedKeys.Add(Key.KeyHandle);

	if (TViewModelPtr<IOutlinerExtension> OutlinerItem = Key.WeakChannel.Pin()->GetLinkedOutlinerItem())
	{
		OutlinerItem->ToggleSelectionState(EOutlinerSelectionState::HasSelectedKeys, true);
	}

	if ( IsBroadcasting() )
	{
		OnKeySelectionChanged.Broadcast();
		OnOutlinerNodeSelectionChangedObjectGuids.Broadcast();

		// Deselect any outliner nodes that aren't within the trunk of this key
		TArray<UMovieSceneSection*> Sections;
		Sections.Add(Key.Section);

		EmptySelectedOutlinerNodesWithoutSections(Sections);
	}
	else
	{	
		bEmptySelectedOutlinerNodesWithSectionsPending = true;
	}
}

void FSequencerSelection::AddToOutlinerSelection(TSharedPtr<UE::Sequencer::FViewModel> InModel)
{
	if (!ensureAlwaysMsgf(InModel, TEXT("AddToSelection with null view model")))
	{
		return;
	}

	using namespace UE::Sequencer;

	ISelectableExtension* Selectable = InModel->CastThis<ISelectableExtension>();
	if (Selectable && Selectable->IsSelectable() == ESelectionIntent::Never)
	{
		return;
	}

	++SerialNumber;

	SelectedOutlinerItems.Add(InModel);
	if (IOutlinerExtension* OutlinerItem = InModel->CastThis<IOutlinerExtension>())
	{
		OutlinerItem->SetSelectionState(EOutlinerSelectionState::SelectedDirectly);
	}

	if ( IsBroadcasting() )
	{
		OnOutlinerNodeSelectionChanged.Broadcast();
		OnOutlinerNodeSelectionChangedObjectGuids.Broadcast();
	}

	EmptySelectedKeys();
	EmptySelectedTrackAreaItems();
}

void FSequencerSelection::AddToSelection(const TArrayView<TSharedPtr<UE::Sequencer::FViewModel>>& InModels)
{
	using namespace UE::Sequencer;

	++SerialNumber;

	for (TSharedPtr<FViewModel> InModel : InModels)
	{
		if (ensureAlwaysMsgf(InModel, TEXT("AddToSelection with null view model")))
		{
			SelectedOutlinerItems.Add(InModel);
			if (IOutlinerExtension* OutlinerItem = InModel->CastThis<IOutlinerExtension>())
			{
				OutlinerItem->SetSelectionState(EOutlinerSelectionState::SelectedDirectly);
			}
		}
	}

	if (IsBroadcasting())
	{
		OnOutlinerNodeSelectionChanged.Broadcast();
		OnOutlinerNodeSelectionChangedObjectGuids.Broadcast();
	}

	EmptySelectedKeys();
	EmptySelectedTrackAreaItems();
}

void FSequencerSelection::AddToSelection(const TArrayView<TSharedRef<UE::Sequencer::FViewModel>>& InModels)
{
	using namespace UE::Sequencer;

	++SerialNumber;

	for (TSharedRef<FViewModel> InModel : InModels)
	{
		SelectedOutlinerItems.Add(InModel);
		if (IOutlinerExtension* OutlinerItem = InModel->CastThis<IOutlinerExtension>())
		{
			OutlinerItem->SetSelectionState(EOutlinerSelectionState::SelectedDirectly);
		}
	}

	if (IsBroadcasting())
	{
		OnOutlinerNodeSelectionChanged.Broadcast();
		OnOutlinerNodeSelectionChangedObjectGuids.Broadcast();
	}

	EmptySelectedKeys();
	EmptySelectedTrackAreaItems();
}


void FSequencerSelection::RemoveFromSelection(const FSequencerSelectedKey& Key)
{
	using namespace UE::Sequencer;

	bNodesWithSelectedKeysOrSectionsDirty = true;
	++SerialNumber;

	SelectedKeys.Remove(Key);
	RawSelectedKeys.Remove(Key.KeyHandle);

	if (TViewModelPtr<IOutlinerExtension> OutlinerItem = Key.WeakChannel.Pin()->GetLinkedOutlinerItem())
	{
		OutlinerItem->ToggleSelectionState(EOutlinerSelectionState::HasSelectedKeys, false);
	}

	if ( IsBroadcasting() )
	{
		OnKeySelectionChanged.Broadcast();
	}
}

void FSequencerSelection::RemoveFromSelection(TWeakPtr<UE::Sequencer::FViewModel> InModel)
{
	if (TSharedPtr<UE::Sequencer::FViewModel> InModelPtr = InModel.Pin())
	{
		RemoveFromSelection(InModelPtr);
	}
}

void FSequencerSelection::RemoveFromSelection(TSharedPtr<UE::Sequencer::FViewModel> OutlinerNode)
{
	if (OutlinerNode)
	{
		RemoveFromSelection(OutlinerNode.ToSharedRef());
	}
}

void FSequencerSelection::RemoveFromSelection(TSharedRef<UE::Sequencer::FViewModel> OutlinerNode)
{
	using namespace UE::Sequencer;

	bNodesWithSelectedKeysOrSectionsDirty = true;
	++SerialNumber;

	SelectedOutlinerItems.Remove(OutlinerNode);
	SelectedTrackAreaItems.Remove(OutlinerNode);

	if (IOutlinerExtension* OutlinerItem = OutlinerNode->CastThis<IOutlinerExtension>())
	{
		OutlinerItem->SetSelectionState(EOutlinerSelectionState::None);
	}

	if ( IsBroadcasting() )
	{
		OnOutlinerNodeSelectionChanged.Broadcast();
	}
}

bool FSequencerSelection::IsSelected(const FSequencerSelectedKey& Key) const
{
	return SelectedKeys.Contains(Key);
}

bool FSequencerSelection::IsSelected(TWeakPtr<UE::Sequencer::FViewModel> InModel) const
{
	return SelectedOutlinerItems.Contains(InModel) || SelectedTrackAreaItems.Contains(InModel);
}

bool FSequencerSelection::NodeHasSelectedKeysOrSections(TWeakPtr<UE::Sequencer::FViewModel> InModel) const
{
	return GetNodesWithSelectedKeysOrSections().Contains(InModel);
}

void FSequencerSelection::Empty()
{
	using namespace UE::Sequencer;

	bNodesWithSelectedKeysOrSectionsDirty = true;
	++SerialNumber;

	FSectionModelStorageExtension* SectionModelStorage = RootModel->CastDynamicChecked<FSectionModelStorageExtension>();
	for (const FSequencerSelectedKey& Key : SelectedKeys)
	{
		if (TSharedPtr<FSectionModel> SectionModel = SectionModelStorage->FindModelForSection(Key.Section))
		{
			for (TSharedPtr<IOutlinerExtension> ParentItem : SectionModel->GetAncestorsOfType<IOutlinerExtension>(true))
			{
				ParentItem->SetSelectionState(EOutlinerSelectionState::None);
			}
		}
	}
	for (TWeakPtr<FViewModel> WeakModel : SelectedTrackAreaItems)
	{
		if (TSharedPtr<FViewModel> Model = WeakModel.Pin())
		{
			for (TSharedPtr<IOutlinerExtension> ParentOutlinerItem : Model->GetAncestorsOfType<IOutlinerExtension>())
			{
				ParentOutlinerItem->SetSelectionState(EOutlinerSelectionState::None);
			}
		}
	}
	for (TWeakPtr<FViewModel> WeakModel : SelectedOutlinerItems)
	{
		if (TSharedPtr<FViewModel> Model = WeakModel.Pin())
		{
			if (IOutlinerExtension* OutlinerItem = Model->CastThis<IOutlinerExtension>())
			{
				OutlinerItem->SetSelectionState(EOutlinerSelectionState::None);
			}
		}
	}

	EmptySelectedKeys();
	EmptySelectedOutlinerNodes();
	EmptySelectedTrackAreaItems();
}

void FSequencerSelection::EmptySelectedKeys()
{
	using namespace UE::Sequencer;

	if (!SelectedKeys.Num())
	{
		return;
	}

	bNodesWithSelectedKeysOrSectionsDirty = true;
	++SerialNumber;

	FSectionModelStorageExtension* SectionModelStorage = RootModel->CastDynamicChecked<FSectionModelStorageExtension>();
	for (const FSequencerSelectedKey& Key : SelectedKeys)
	{
		if (TViewModelPtr<IOutlinerExtension> OutlinerItem = Key.WeakChannel.Pin()->GetLinkedOutlinerItem())
		{
			OutlinerItem->ToggleSelectionState(EOutlinerSelectionState::HasSelectedKeys, false);
		}
	}
	SelectedKeys.Empty();
	RawSelectedKeys.Empty();

	if ( IsBroadcasting() )
	{
		OnKeySelectionChanged.Broadcast();
	}
}

void FSequencerSelection::EmptySelectedTrackAreaItems()
{
	using namespace UE::Sequencer;

	if (SelectedTrackAreaItems.Num() > 0)
	{
		bNodesWithSelectedKeysOrSectionsDirty = true;
		++SerialNumber;

		for (TWeakPtr<FViewModel> WeakModel : SelectedTrackAreaItems)
		{
			if (TSharedPtr<FViewModel> Model = WeakModel.Pin())
			{
				for (TSharedPtr<IOutlinerExtension> ParentOutlinerItem : Model->GetAncestorsOfType<IOutlinerExtension>())
				{
					ParentOutlinerItem->SetSelectionState(EOutlinerSelectionState::None);
				}
			}
		}
		SelectedTrackAreaItems.Empty();

		if ( IsBroadcasting() )
		{
			OnSectionSelectionChanged.Broadcast();
		}
	}
}

void FSequencerSelection::EmptySelectedOutlinerNodes()
{
	using namespace UE::Sequencer;

	if (!SelectedOutlinerItems.Num())
	{
		return;
	}

	++SerialNumber;

	for (TWeakPtr<FViewModel> WeakModel : SelectedOutlinerItems)
	{
		if (TSharedPtr<FViewModel> Model = WeakModel.Pin())
		{
			if (IOutlinerExtension* OutlinerItem = Model->CastThis<IOutlinerExtension>())
			{
				OutlinerItem->SetSelectionState(EOutlinerSelectionState::None);
			}
		}
	}
	SelectedOutlinerItems.Empty();

	if ( IsBroadcasting() )
	{
		OnOutlinerNodeSelectionChanged.Broadcast();
	}
}

/** Suspend or resume broadcast of selection changing  */
void FSequencerSelection::SuspendBroadcast()
{
	SuspendBroadcastCount++;
}

void FSequencerSelection::ResumeBroadcast()
{
	SuspendBroadcastCount--;
	checkf(SuspendBroadcastCount >= 0, TEXT("Suspend/Resume broadcast mismatch!"));
}

bool FSequencerSelection::IsBroadcasting() 
{
	return SuspendBroadcastCount == 0;
}

void FSequencerSelection::EmptySelectedOutlinerNodesWithoutSections(const TArray<UMovieSceneSection*>& Sections)
{
	using namespace UE::Sequencer;

	TSet<TWeakPtr<UE::Sequencer::FViewModel>> LocalSelectedOutlinerItems = SelectedOutlinerItems;

	SuspendBroadcast();
	bool bRemoved = false;
	for (TWeakPtr<FViewModel> WeakSelectedModel : LocalSelectedOutlinerItems)
	{
		TSharedPtr<FViewModel> SelectedModel = WeakSelectedModel.Pin();
		if (!SelectedModel)
		{
			continue;
		}

		bool bFoundMatch = false;

		if (TSharedPtr<FTrackModel> TrackModel = SelectedModel->FindAncestorOfType<FTrackModel>())
		{
			if (UMovieSceneTrack* Track = TrackModel->GetTrack())
			{
				for (int32 SectionIndex = 0; SectionIndex < Track->GetAllSections().Num() && !bFoundMatch; ++SectionIndex)
				{
					if (Sections.Contains(Track->GetAllSections()[SectionIndex]))
					{
						bFoundMatch = true;
						break;
					}
				}
			}
		}
			
		for (TSharedPtr<FSectionModel> SectionModel : SelectedModel->GetDescendantsOfType<FSectionModel>())
		{
			if (Sections.Contains(SectionModel->GetSection()))
			{
				bFoundMatch = true;
				break;
			}
		}

		if (!bFoundMatch)
		{
			RemoveFromSelection(WeakSelectedModel);
			bRemoved = true;
		}
	}
	ResumeBroadcast();

	if (bRemoved)
	{
		++SerialNumber;

		OnOutlinerNodeSelectionChanged.Broadcast();
	}
}

void FSequencerSelection::RequestOutlinerNodeSelectionChangedBroadcast()
{
	bOutlinerNodeSelectionChangedBroadcastPending = true;
}

void FSequencerSelection::Tick()
{
	if ( bOutlinerNodeSelectionChangedBroadcastPending && IsBroadcasting() )
	{
		bOutlinerNodeSelectionChangedBroadcastPending = false;
		OnOutlinerNodeSelectionChanged.Broadcast();
	}

	if ( bEmptySelectedOutlinerNodesWithSectionsPending && IsBroadcasting() )
	{
		bEmptySelectedOutlinerNodesWithSectionsPending = false;

		TArray<UMovieSceneSection*> Sections;
		for (TWeakObjectPtr<UMovieSceneSection> SelectedSection : GetSelectedSections())
		{
			if (SelectedSection.IsValid())
			{
				Sections.Add(SelectedSection.Get());
			}
		}

		for (FSequencerSelectedKey SelectedKey : SelectedKeys)
		{
			if (SelectedKey.IsValid() && !Sections.Contains(SelectedKey.Section))
			{
				Sections.Add(SelectedKey.Section);
			}
		}

		EmptySelectedOutlinerNodesWithoutSections(Sections);
	}
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
	using namespace UE::Sequencer;

	bNodesWithSelectedKeysOrSectionsDirty = true;

	const int32 NumSelectedKeys = SelectedKeys.Num();
	const int32 NumSelectedTrackAreaItems = SelectedTrackAreaItems.Num();
	const int32 NumSelectedOutlinerItems = SelectedOutlinerItems.Num();

	for (auto It = SelectedKeys.CreateIterator(); It; ++It)
	{
		if (!It->Section || !It->WeakChannel.Pin().Get())
		{
			RawSelectedKeys.Remove(It->KeyHandle);
			It.RemoveCurrent();
		}
	}

	for (auto It = SelectedTrackAreaItems.CreateIterator(); It; ++It)
	{
		if (It->Pin().Get() == nullptr)
		{
			It.RemoveCurrent();
		}
	}

	for (auto It = SelectedOutlinerItems.CreateIterator(); It; ++It)
	{
		if (It->Pin().Get() == nullptr)
		{
			It.RemoveCurrent();
		}
	}

	if (NumSelectedKeys != SelectedKeys.Num())
	{
		++SerialNumber;

		OnKeySelectionChanged.Broadcast();
	}

	if (NumSelectedTrackAreaItems != SelectedTrackAreaItems.Num())
	{
		++SerialNumber;

		OnSectionSelectionChanged.Broadcast();
		OnOutlinerNodeSelectionChangedObjectGuids.Broadcast();
	}

	if (NumSelectedOutlinerItems != SelectedOutlinerItems.Num())
	{
		++SerialNumber;
		OnOutlinerNodeSelectionChanged.Broadcast();
	}
}

