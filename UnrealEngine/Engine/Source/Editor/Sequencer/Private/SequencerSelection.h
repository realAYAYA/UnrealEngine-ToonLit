// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Containers/Set.h"
#include "Delegates/Delegate.h"
#include "HAL/Platform.h"
#include "HAL/PlatformCrt.h"
#include "MVVM/ViewModelPtr.h"
#include "MVVM/ViewModels/ViewModel.h"
#include "MVVM/ViewModels/ViewModelIterators.h"
#include "SequencerSelectedKey.h"
#include "Templates/SharedPointer.h"
#include "UObject/WeakObjectPtrTemplates.h"

class UMovieSceneSection;
class UMovieSceneTrack;
struct FGuid;

/**
 * Manages the selection of keys, sections, and outliner nodes for the sequencer.
 */
class FSequencerSelection
{
public:

	using FViewModel = UE::Sequencer::FViewModel;

	DECLARE_MULTICAST_DELEGATE(FOnSelectionChanged)
	DECLARE_MULTICAST_DELEGATE(FOnSelectionChangedObjectGuids)

public:

	FSequencerSelection();

	TSharedPtr<FViewModel> GetRootModel() const;
	void SetRootModel(TSharedPtr<FViewModel> InRootModel);

	/** Gets a set of the selected keys. */
	const TSet<FSequencerSelectedKey>& GetSelectedKeys() const;

	/** Gets a set of the selected keys. */
	const TSet<FKeyHandle>& GetRawSelectedKeys() const;

	/** Gets a set of the selected sections */
	TSet<TWeakObjectPtr<UMovieSceneSection>> GetSelectedSections() const;

	/** Gets a set of the selected models on the outliner */
	const TSet<TWeakPtr<FViewModel>>& GetSelectedOutlinerItems() const;

	/** Gets the selected models on the outliner */
	void GetSelectedOutlinerItems(TArray<TSharedRef<FViewModel>>& OutItems) const
	{
		OutItems.Reserve(SelectedOutlinerItems.Num());

		for (const TWeakPtr<FViewModel>& WeakItem : SelectedOutlinerItems)
		{
			if (TSharedPtr<FViewModel> Item = WeakItem.Pin())
			{
				OutItems.Add(Item.ToSharedRef());
			}
		}
	}

	void GetSelectedOutlinerItems(TArray<TSharedPtr<FViewModel>>& OutItems) const
	{
		OutItems.Reserve(SelectedOutlinerItems.Num());

		for (const TWeakPtr<FViewModel>& WeakItem : SelectedOutlinerItems)
		{
			if (TSharedPtr<FViewModel> Item = WeakItem.Pin())
			{
				OutItems.Add(Item);
			}
		}
	}

	/** Gets a set of the selected models */
	const TSet<TWeakPtr<FViewModel>>& GetSelectedTrackAreaItems() const;

	/** Gets a set of the outliner nodes that have selected keys or sections */
	const TSet<TWeakPtr<FViewModel>>& GetNodesWithSelectedKeysOrSections() const;

	/** Gets the outliner nodes that have selected keys or sections */
	void GetNodesWithSelectedKeysOrSections(TArray<TSharedRef<FViewModel>>& OutKeysOrSections) const
	{
		const TSet<TWeakPtr<FViewModel>>& NodesWithKeysOrSections = GetNodesWithSelectedKeysOrSections();

		OutKeysOrSections.Reserve(OutKeysOrSections.Num() + NodesWithKeysOrSections.Num());

		for (const TWeakPtr<FViewModel>& WeakItem : NodesWithKeysOrSections)
		{
			if (TSharedPtr<FViewModel> Item = WeakItem.Pin())
			{
				OutKeysOrSections.Add(Item.ToSharedRef());
			}
		}
	}

	/** Get the currently selected tracks as UMovieSceneTracks */
	TArray<UMovieSceneTrack*> GetSelectedTracks() const;

	/** Adds a key to the selection */
	void AddToSelection(const FSequencerSelectedKey& Key);

	/** Adds a model to the selection */
	void AddToSelection(TSharedPtr<FViewModel> InModel);

	/** Adds models to the selection */
	void AddToSelection(const TArrayView<TSharedPtr<FViewModel>>& InModels);

	/** Adds models to the selection */
	void AddToSelection(const TArrayView<TSharedRef<FViewModel>>& InModels);

	/** Adds a model to the selection */
	void AddToTrackAreaSelection(TSharedPtr<FViewModel> InModel);
	void AddToOutlinerSelection(TSharedPtr<FViewModel> InModel);

	/** Removes a key from the selection */
	void RemoveFromSelection(const FSequencerSelectedKey& Key);

	/** Removes a model from the selection */
	void RemoveFromSelection(TWeakPtr<FViewModel> InModel);

	/** Removes an outliner node from the selection */
	void RemoveFromSelection(TSharedPtr<FViewModel> OutlinerNode);

	/** Removes an outliner node from the selection */
	void RemoveFromSelection(TSharedRef<FViewModel> OutlinerNode);

	/** Removes any outliner nodes from the selection that do not relate to the given section */
	void EmptySelectedOutlinerNodesWithoutSections(const TArray<UMovieSceneSection*>& Sections);

	/** Returns whether or not the key is selected. */
	bool IsSelected(const FSequencerSelectedKey& Key) const;

	/** Returns whether or not the model is selected. */
	bool IsSelected(TWeakPtr<FViewModel> InModel) const;

	/** Returns whether or not the outliner node has keys or sections selected. */
	bool NodeHasSelectedKeysOrSections(TWeakPtr<FViewModel> Model) const;

	/** Empties all selections. */
	void Empty();

	/** Empties the key selection. */
	void EmptySelectedKeys();

	/** Empties the Track Area selection. */
	void EmptySelectedTrackAreaItems();

	/** Empties the outliner node selection. */
	void EmptySelectedOutlinerNodes();

	/** Gets a multicast delegate which is called when the key selection changes. */
	FOnSelectionChanged& GetOnKeySelectionChanged();

	/** Gets a multicast delegate which is called when the section selection changes. */
	FOnSelectionChanged& GetOnSectionSelectionChanged();

	/** Gets a multicast delegate which is called when the outliner node selection changes. */
	FOnSelectionChanged& GetOnOutlinerNodeSelectionChanged();

	/** Gets a multicast delegate with an array of FGuid of bound objects which is called when the outliner node selection changes. */
	FOnSelectionChangedObjectGuids& GetOnOutlinerNodeSelectionChangedObjectGuids();

	/** Helper function to get an array of FGuid of bound objects on return */
	TArray<FGuid> GetBoundObjectsGuids();

	/** Suspend the broadcast of selection change notifications.  */
	void SuspendBroadcast();

	/** Resume the broadcast of selection change notifications.  */
	void ResumeBroadcast();

	/** Requests that the outliner node selection changed delegate be broadcast on the next update. */
	void RequestOutlinerNodeSelectionChangedBroadcast();

	/** Updates the selection once per frame.  This is required for deferred selection broadcasts. */
	void Tick();

	/** Revalidates the selection by removing stale ptrs */
	void RevalidateSelection();

	/** When true, selection change notifications should be broadcasted. */
	bool IsBroadcasting();

	/** Retrieve the serial number that identifies this selection's state. */
	uint32 GetSerialNumber() const
	{
		return SerialNumber;
	}

private:

	void OnHierarchyChanged();

private:

	TSharedPtr<FViewModel> RootModel;

	TSet<FSequencerSelectedKey> SelectedKeys;
	/** Selected keys added directly by FKeyHandle, derived from SelectedKeys on mutation */
	TSet<FKeyHandle> RawSelectedKeys;
	TSet<TWeakPtr<FViewModel>> SelectedTrackAreaItems;
	TSet<TWeakPtr<FViewModel>> SelectedOutlinerItems;
	mutable TSet<TWeakPtr<FViewModel>> NodesWithSelectedKeysOrSections;

	FOnSelectionChanged OnKeySelectionChanged;
	FOnSelectionChanged OnSectionSelectionChanged;
	FOnSelectionChanged OnOutlinerNodeSelectionChanged;

	FOnSelectionChangedObjectGuids OnOutlinerNodeSelectionChangedObjectGuids;

	/** Serial number that is incremented whenever the state of this selection has changed. */
	uint32 SerialNumber;

	/** The number of times the broadcasting of selection change notifications has been suspended. */
	int32 SuspendBroadcastCount;

	/** Delegate handle for the current root node on-hierarchy-changed event */
	FDelegateHandle HierarchyChangedHandle;

	/** When true there is a pending outliner node selection change which will be broadcast next tick. */
	bool bOutlinerNodeSelectionChangedBroadcastPending;

	/** When true there is a pending call to remove selected outliner nodes that don't have sections selected. */
	bool bEmptySelectedOutlinerNodesWithSectionsPending;

	mutable bool bNodesWithSelectedKeysOrSectionsDirty;
};

