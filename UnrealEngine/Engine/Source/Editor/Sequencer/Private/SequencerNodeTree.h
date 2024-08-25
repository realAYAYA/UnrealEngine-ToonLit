// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MVVM/ViewModels/ViewModelHierarchy.h"
#include "Misc/Guid.h"
#include "UObject/ObjectKey.h"
#include "MVVM/ViewModels/ViewModel.h"
#include "MVVM/ViewModels/ObjectBindingModel.h"
#include "MVVM/ViewModels/TrackModel.h"
#include "MVVM/ViewModels/SectionModel.h"
#include "MVVM/ViewModels/OutlinerItemModel.h"
#include "MVVM/ViewModels/ChannelModel.h"
#include "MVVM/ViewModels/SequenceModel.h"
#include "MVVM/Extensions/IOutlinerExtension.h"
#include "MovieSceneSequence.h"

class FSequencer;
class FSequencerTrackFilter;
class FSequencerTrackFilterCollection;
class FSequencerTrackFilter_LevelFilter;
class ISequencerTrackEditor;
class UMovieScene;
class UMovieSceneFolder;
class UMovieSceneTrack;
struct FMovieSceneBinding;

/**
 * Represents a tree of sequencer display nodes, used to populate the Sequencer UI with MovieScene data
 */
class FSequencerNodeTree : public TSharedFromThis<FSequencerNodeTree>
{
public:

	DECLARE_MULTICAST_DELEGATE(FOnUpdated);

	using FSectionModel = UE::Sequencer::FSectionModel;
	using FViewModel = UE::Sequencer::FViewModel;
	using FObjectBindingModel = UE::Sequencer::FObjectBindingModel;

public:

	FSequencerNodeTree(FSequencer& InSequencer);
	
	~FSequencerNodeTree();

	/**
	 * Updates the tree with sections from a MovieScene
	 */
	void Update();

	/**
	 * Set this tree's symbolic root node
	 */
	void SetRootNode(const UE::Sequencer::FViewModelPtr& InRootNode);

	/**
	 * Access this tree's symbolic root node
	 */
	UE::Sequencer::FViewModelPtr GetRootNode() const;

	/**
	 * @return The root nodes of the tree
	 */
	TArray<UE::Sequencer::TViewModelPtr<UE::Sequencer::IOutlinerExtension>> GetRootNodes() const;

	/** @return Whether or not there is an active filter */
	bool HasActiveFilter() const;

	/** 
	 * Checks if filters should be updated on track value changes, and if so updates them.
	 * @return Whether the filtered node list was modified
	 */
	bool UpdateFiltersOnTrackValueChanged();

	/**
	 * Returns whether or not a node is filtered
	 *
	 * @param Node	The node to check if it is filtered
	 */
	bool IsNodeFiltered(const TSharedPtr<FViewModel>& Node) const;

	/**
	 * Schedules an update of all filters
	 */
	void RequestFilterUpdate() { bFilterUpdateRequested = true; }

	/**
	 * @return Whether there is a filter update scheduled
	 */
	bool NeedsFilterUpdate() const { return bFilterUpdateRequested; }

	/**
	 * Filters the nodes based on the passed in filter terms
	 *
	 * @param InFilter	The filter terms
	 */
	void FilterNodes( const FString& InFilter );

	/** Called when the active MovieScene's node group colletion has been modifed */
	void NodeGroupsCollectionChanged();

	/**
	 * @return All nodes in a flat array
	 */
	void GetAllNodes(TArray<TSharedPtr<FViewModel>>& OutNodes) const;

	/**
	 * @return All nodes in a flat array
	 */
	void GetAllNodes(TArray<TSharedRef<FViewModel>>& OutNodes) const;

	/** Gets the parent sequencer of this tree */
	FSequencer& GetSequencer() {return Sequencer;}

	/**
	 * Saves the expansion state of a display node
	 *
	 * @param Node		The node whose expansion state should be saved
	 * @param bExpanded	The new expansion state of the node
	 */
	void SaveExpansionState( const FViewModel& Node, bool bExpanded );

	/**
	 * Gets the saved expansion state of a display node
	 *
	 * @param Node	The node whose expansion state may have been saved
	 * @return true if the node should be expanded, false otherwise, unset if it wasn't saved
	 */
	TOptional<bool> GetSavedExpansionState( const FViewModel& Node );

	/**
	 * Saves the pinned state of a display node
	 *
	 * @param Node		The node whose pinned state should be saved
	 * @param bPinned	The new pinned state of the node
	 */
	void SavePinnedState( const FViewModel& Node, bool bPinned );

	/**
	 * Gets the saved pinned state of a display node
	 *
	 * @param Node	The node whose pinned state may have been saved
	 * @return true if the node should be pinned, false otherwise	
	 */
	bool GetSavedPinnedState( const FViewModel& Node ) const;

	/*
	 * Find the object binding node with the specified GUID
	 */
	TSharedPtr<FObjectBindingModel> FindObjectBindingNode(const FGuid& BindingID) const;

	/*
	 * Gets a multicast delegate which is called whenever the node tree has been updated.
	 */
	FOnUpdated& OnUpdated() { return OnUpdatedDelegate; }

	/** Clears all custom sort orders so that sorting will be by category then alphabetically. */
	void ClearCustomSortOrders();

	/** Sorts all nodes and their descendants*/
	void SortAllNodesAndDescendants();

	/**
	 * Attempt to get a node at the specified path
	 *
	 * @param NodePath The path of the node to search for
	 * @return The node located at NodePath, or nullptr if not found
	 */
	UE::Sequencer::TViewModelPtr<UE::Sequencer::IOutlinerExtension> GetNodeAtPath(const FString& NodePath) const;

public:

	/**
	 * Finds the section handle relating to the specified section object, or Null if one was not found (perhaps, if it's filtered away)
	 */
	TSharedPtr<FSectionModel> GetSectionModel(const UMovieSceneSection* Section) const;

	void AddFilter(TSharedPtr<FSequencerTrackFilter> TrackFilter);
	int32 RemoveFilter(TSharedPtr<FSequencerTrackFilter> TrackFilter);
	void RemoveAllFilters();
	bool IsTrackFilterActive(TSharedPtr<FSequencerTrackFilter> TrackFilter) const;

	void AddLevelFilter(const FString& LevelName);
	void RemoveLevelFilter(const FString& LevelName);
	bool IsTrackLevelFilterActive(const FString& LevelName) const;

private:

	/** Returns whether this NodeTree should only display selected nodes */
	bool ShowSelectedNodesOnly() const;

private:

	/**
	 * Update the list of filters nodes based on current filter settings, if an update is scheduled
	 * This is called by Update();
	 * 
	 * @return Whether the list of filtered nodes changed
	 */
	bool UpdateFilters();

	/**
	 * Cleans outdated mute/solo markers
	 */
	void CleanupMuteSolo(UMovieScene* MovieScene);

public:

	int32 GetTotalDisplayNodeCount() const;
	int32 GetFilteredDisplayNodeCount() const;

private:

	/** Symbolic root node that contains the actual displayed root nodes as children */
	UE::Sequencer::FViewModelPtr RootNode;

	/** Set of all filtered nodes */
	TSet<UE::Sequencer::TWeakViewModelPtr<UE::Sequencer::IOutlinerExtension>> FilteredNodes;
	/** Active filter string if any */
	FString FilterString;
	/** Sequencer interface */
	FSequencer& Sequencer;
	/** A multicast delegate which is called whenever the node tree has been updated. */
	FOnUpdated OnUpdatedDelegate;

	/** Active track filters */
	TSharedPtr<FSequencerTrackFilterCollection> TrackFilters;
	
	/** Level based track filtering */
	TSharedPtr<FSequencerTrackFilter_LevelFilter> TrackFilterLevelFilter;

	/** The total number of DisplayNodes in the tree, both displayed and hidden */
	uint32 DisplayNodeCount;

	bool bFilterUpdateRequested;
	bool bFilteringOnNodeGroups;
};

