// Copyright Epic Games, Inc. All Rights Reserved.

#include "SequencerNodeTree.h"
#include "MVVM/ViewModels/ViewModelHierarchy.h"
#include "MVVM/Selection/Selection.h"
#include "MovieSceneBinding.h"
#include "GameFramework/Actor.h"
#include "MovieScene.h"
#include "MVVM/ViewModels/ViewModel.h"
#include "MVVM/Extensions/IOutlinerExtension.h"
#include "MVVM/Extensions/ICurveEditorTreeItemExtension.h"
#include "MVVM/Extensions/IPinnableExtension.h"
#include "MVVM/Extensions/ISoloableExtension.h"
#include "MVVM/Extensions/IMutableExtension.h"
#include "MVVM/ViewModels/SequencerEditorViewModel.h"
#include "MVVM/ViewModels/OutlinerViewModel.h"
#include "MVVM/ViewModels/CategoryModel.h"
#include "MVVM/ViewModels/FolderModel.h"
#include "MVVM/ViewModels/ViewModelIterators.h"
#include "MVVM/ViewModels/ObjectBindingModel.h"
#include "MVVM/ViewModels/OutlinerSpacer.h"
#include "MVVM/ViewModels/SectionModel.h"
#include "MVVM/ViewModels/SequenceModel.h"
#include "MVVM/ViewModels/TrackModel.h"
#include "MVVM/CurveEditorIntegrationExtension.h"
#include "MVVM/ObjectBindingModelStorageExtension.h"
#include "MVVM/SectionModelStorageExtension.h"
#include "MVVM/TrackModelStorageExtension.h"
#include "IKeyArea.h"
#include "ISequencerSection.h"
#include "MovieSceneSequence.h"
#include "Sequencer.h"
#include "MovieSceneFolder.h"
#include "ISequencerTrackEditor.h"
#include "Widgets/Views/STableRow.h"
#include "CurveEditor.h"
#include "SequencerTrackFilters.h"
#include "Channels/MovieSceneChannel.h"
#include "ScopedTransaction.h"
#include "SequencerUtilities.h"
#include "SequencerLog.h"
#include "SequencerCommonHelpers.h"

FSequencerNodeTree::~FSequencerNodeTree()
{
	if (TrackFilters.IsValid())
	{
		TrackFilters->OnChanged().RemoveAll(this);
	}
	if (TrackFilterLevelFilter.IsValid())
	{
		TrackFilterLevelFilter->OnChanged().RemoveAll(this);
	}
}

FSequencerNodeTree::FSequencerNodeTree(FSequencer& InSequencer)
	: Sequencer(InSequencer)
	, DisplayNodeCount(0)
	, bFilterUpdateRequested(false)
{
	TrackFilters = MakeShared<FSequencerTrackFilterCollection>();
	TrackFilters->OnChanged().AddRaw(this, &FSequencerNodeTree::RequestFilterUpdate);
	TrackFilterLevelFilter = MakeShared< FSequencerTrackFilter_LevelFilter>();
	TrackFilterLevelFilter->OnChanged().AddRaw(this, &FSequencerNodeTree::RequestFilterUpdate);
}

TSharedPtr<UE::Sequencer::FObjectBindingModel> FSequencerNodeTree::FindObjectBindingNode(const FGuid& BindingID) const
{
	using namespace UE::Sequencer;

	const FObjectBindingModelStorageExtension* ObjectBindingStorage = RootNode->CastDynamic<FObjectBindingModelStorageExtension>();
	if (ObjectBindingStorage)
	{
		TSharedPtr<FObjectBindingModel> Model = ObjectBindingStorage->FindModelForObjectBinding(BindingID);
		return Model;
	}

	return nullptr;
}

bool FSequencerNodeTree::HasActiveFilter() const
{
	return (!FilterString.IsEmpty()
		|| TrackFilters->Num() > 0
		|| TrackFilterLevelFilter->IsActive()
		|| Sequencer.GetSequencerSettings()->GetShowSelectedNodesOnly()
		|| Sequencer.GetFocusedMovieSceneSequence()->GetMovieScene()->GetNodeGroups().HasAnyActiveFilter());
}

bool FSequencerNodeTree::UpdateFiltersOnTrackValueChanged()
{
	// If filters are already scheduled for update, we can defer until the next update
	if (bFilterUpdateRequested)
	{
		return false;
	}

	for (TSharedPtr< FSequencerTrackFilter > TrackFilter : *TrackFilters)
	{
		if (TrackFilter->ShouldUpdateOnTrackValueChanged())
		{
			// UpdateFilters will only run if bFilterUpdateRequested is true
			bFilterUpdateRequested = true;
			bool bFiltersUpdated = UpdateFilters();

			// If the filter list was modified, set bFilterUpdateRequested to suppress excessive re-filters between tree update
			bFilterUpdateRequested = bFiltersUpdated;
			return bFiltersUpdated;
		}
	}

	return false;
}

void FSequencerNodeTree::Update()
{
	using namespace UE::Sequencer;

	FViewModelHierarchyOperation UpdateOp(RootNode);

	FilteredNodes.Empty();

	if (!ensure(RootNode))
	{
		return;
	}

	TSharedPtr<FSequenceModel> SequenceModel = RootNode->CastThisShared<FSequenceModel>();
	check(SequenceModel);

	UMovieSceneSequence* CurrentSequence = SequenceModel->GetSequence();
	check(CurrentSequence);

	UMovieScene* MovieScene = CurrentSequence->GetMovieScene();
	CleanupMuteSolo(MovieScene);

	// Re-filter the tree after updating 
	// @todo sequencer: Newly added sections may need to be visible even when there is a filter
	FilterNodes(FilterString);
	bFilterUpdateRequested = true;
	UpdateFilters();

	// Sort all nodes
	const bool bIncludeRootNode = true;
	for (TSharedPtr<ISortableExtension> SortableChild : RootNode->GetDescendantsOfType<ISortableExtension>(bIncludeRootNode))
	{
		SortableChild->SortChildren();
	}

	// Update all virtual geometries
	// This must happen after the sorting
	IGeometryExtension::UpdateVirtualGeometry(0.f, RootNode);

	// Cache pinned state of nodes, needs to happen after OnTreeRefreshed
	FPinnableExtensionShim::UpdateCachedPinnedState(RootNode);

	// Update curve editor tree based on new filtered hierarchy
	auto CurveEditorIntegration = SequenceModel->CastDynamic<FCurveEditorIntegrationExtension>();
	if (CurveEditorIntegration)
	{
		CurveEditorIntegration->UpdateCurveEditor();
	}

	OnUpdatedDelegate.Broadcast();
}

UE::Sequencer::TViewModelPtr<UE::Sequencer::IOutlinerExtension> FindNodeWithPath(UE::Sequencer::TViewModelPtr<UE::Sequencer::IOutlinerExtension> InNode, const FString& NodePath)
{
	using namespace UE::Sequencer;

	if (!InNode)
	{
		return nullptr;
	}

	FString HeadPath, TailPath;
	const bool bHasDelimiter = NodePath.Split(".", &HeadPath, &TailPath);
	const FString NodeIdentifier = InNode->GetIdentifier().ToString();

	if (bHasDelimiter)
	{
		if (NodeIdentifier != HeadPath)
		{
			// The node we're looking for is not in this sub-branch.
			return nullptr;
		}
	}
	else
	{
		// NodePath is just a name, so simply check with our node's name.
		return (NodeIdentifier == NodePath) ? InNode : nullptr;
	}

	check(bHasDelimiter && !TailPath.IsEmpty());

	for (TViewModelPtr<IOutlinerExtension> Child : InNode.AsModel()->GetChildrenOfType<IOutlinerExtension>())
	{
		TViewModelPtr<IOutlinerExtension> FoundNode = FindNodeWithPath(Child, TailPath);
		if (FoundNode)
		{
			return FoundNode;
		}
	}

	return nullptr;
}


UE::Sequencer::TViewModelPtr<UE::Sequencer::IOutlinerExtension> FSequencerNodeTree::GetNodeAtPath(const FString& NodePath) const
{
	using namespace UE::Sequencer;

	for (const TViewModelPtr<IOutlinerExtension>& RootChild : RootNode->GetChildrenOfType<IOutlinerExtension>())
	{
		TViewModelPtr<IOutlinerExtension> NodeAtPath = FindNodeWithPath(RootChild, NodePath);
		if (NodeAtPath)
		{
			return NodeAtPath;
		}
	}
	return nullptr;
}

void FSequencerNodeTree::SetRootNode(const UE::Sequencer::FViewModelPtr& InRootNode)
{
	ensureMsgf(!RootNode, TEXT("Re-assinging the root node is currently an undefined behavior"));
	RootNode = InRootNode;
}

UE::Sequencer::FViewModelPtr FSequencerNodeTree::GetRootNode() const
{
	return RootNode;
}

TArray<UE::Sequencer::TViewModelPtr<UE::Sequencer::IOutlinerExtension>> FSequencerNodeTree::GetRootNodes() const
{
	using namespace UE::Sequencer;

	TArray<TViewModelPtr<IOutlinerExtension>> RootNodes;
	for (const TViewModelPtr<IOutlinerExtension>& Child : RootNode->GetChildrenOfType<IOutlinerExtension>())
	{
		RootNodes.Add(Child);
	}
	return RootNodes;
}

void FSequencerNodeTree::ClearCustomSortOrders()
{
	using namespace UE::Sequencer;

	const bool bIncludeRootNode = true;
	for (TSharedPtr<FViewModel> Child : RootNode->GetDescendants(bIncludeRootNode))
	{
		if (ISortableExtension* SortableExtension = Child->CastThis<ISortableExtension>())
		{
			SortableExtension->SetCustomOrder(-1);
		}
	}
}

void FSequencerNodeTree::SortAllNodesAndDescendants()
{
	using namespace UE::Sequencer;

	const bool bIncludeRootNode = true;
	TArray<ISortableExtension*> SortableChildren;
	for (TSharedPtr<FViewModel> Child : RootNode->GetDescendants(bIncludeRootNode))
	{
		if (ISortableExtension* SortableExtension = Child->CastThis<ISortableExtension>())
		{
			SortableChildren.Add(SortableExtension);
		}
	}
	for (ISortableExtension* SortableChild : SortableChildren)
	{
		SortableChild->SortChildren();
	}
}

void FSequencerNodeTree::AddFilter(TSharedPtr<FSequencerTrackFilter> TrackFilter)
{
	GetSequencer().GetSequencerSettings()->SetTrackFilterEnabled(TrackFilter->GetDisplayName().ToString(), true);

	TrackFilters->Add(TrackFilter);
}

int32 FSequencerNodeTree::RemoveFilter(TSharedPtr<FSequencerTrackFilter> TrackFilter)
{
	GetSequencer().GetSequencerSettings()->SetTrackFilterEnabled(TrackFilter->GetDisplayName().ToString(), false);

	return TrackFilters->Remove(TrackFilter);
}

void FSequencerNodeTree::RemoveAllFilters()
{
	for (TSharedPtr< FSequencerTrackFilter > TrackFilter : *TrackFilters)
	{
		GetSequencer().GetSequencerSettings()->SetTrackFilterEnabled(TrackFilter->GetDisplayName().ToString(), false);
	}

	TrackFilters->RemoveAll();
	TrackFilterLevelFilter->ResetFilter();
}

bool FSequencerNodeTree::IsTrackFilterActive(TSharedPtr<FSequencerTrackFilter> TrackFilter) const
{
	return TrackFilters->Contains(TrackFilter);
}

void FSequencerNodeTree::AddLevelFilter(const FString& LevelName)
{
	TrackFilterLevelFilter->UnhideLevel(LevelName);
}

void FSequencerNodeTree::RemoveLevelFilter(const FString& LevelName)
{
	TrackFilterLevelFilter->HideLevel(LevelName);
}

bool FSequencerNodeTree::IsTrackLevelFilterActive(const FString& LevelName) const
{
	return !TrackFilterLevelFilter->IsLevelHidden(LevelName);
}

void FSequencerNodeTree::SaveExpansionState(const UE::Sequencer::FViewModel& Node, bool bExpanded)
{
	using namespace UE::Sequencer;

	// @todo Sequencer - This should be moved to the sequence level
	UMovieScene* MovieScene = Sequencer.GetFocusedMovieSceneSequence()->GetMovieScene();
	FMovieSceneEditorData& EditorData = MovieScene->GetEditorData();

	EditorData.ExpansionStates.Add(IOutlinerExtension::GetPathName(Node), FMovieSceneExpansionState(bExpanded));
}

TOptional<bool> FSequencerNodeTree::GetSavedExpansionState( const FViewModel& Node )
{
	using namespace UE::Sequencer;

	UMovieScene* MovieScene = Sequencer.GetFocusedMovieSceneSequence()->GetMovieScene();
	FMovieSceneEditorData& EditorData = MovieScene->GetEditorData();
	FMovieSceneExpansionState* ExpansionState = EditorData.ExpansionStates.Find(IOutlinerExtension::GetPathName(Node));

	return ExpansionState ? ExpansionState->bExpanded : TOptional<bool>();
}

void FSequencerNodeTree::SavePinnedState(const UE::Sequencer::FViewModel& Node, bool bPinned)
{
	using namespace UE::Sequencer;

	UMovieScene* MovieScene = Sequencer.GetFocusedMovieSceneSequence()->GetMovieScene();
	FMovieSceneEditorData& EditorData = MovieScene->GetEditorData();

	if (bPinned)
	{
		EditorData.PinnedNodes.AddUnique(IOutlinerExtension::GetPathName(Node));
	}
	else
	{
		EditorData.PinnedNodes.RemoveSingle(IOutlinerExtension::GetPathName(Node));
	}
}


bool FSequencerNodeTree::GetSavedPinnedState(const UE::Sequencer::FViewModel& Node) const
{
	using namespace UE::Sequencer;

	UMovieScene* MovieScene = Sequencer.GetFocusedMovieSceneSequence()->GetMovieScene();
	FMovieSceneEditorData& EditorData = MovieScene->GetEditorData();
	bool bPinned = EditorData.PinnedNodes.Contains(IOutlinerExtension::GetPathName(Node));

	return bPinned;
}

bool FSequencerNodeTree::IsNodeFiltered(const TSharedPtr<UE::Sequencer::FViewModel>& Node) const
{
	using namespace UE::Sequencer;

	TViewModelPtr<IOutlinerExtension> OutlinerItem = CastViewModel<IOutlinerExtension>(Node);
	return OutlinerItem && FilteredNodes.Contains(OutlinerItem);
}

TSharedPtr<UE::Sequencer::FSectionModel> FSequencerNodeTree::GetSectionModel(const UMovieSceneSection* Section) const
{
	using namespace UE::Sequencer;

	FSectionModelStorageExtension* SectionStorage = RootNode->CastThis<FSectionModelStorageExtension>();
	if (ensure(SectionStorage))
	{
		return SectionStorage->FindModelForSection(Section);
	}
	return nullptr;
}

static void AddChildNodes(const UE::Sequencer::TViewModelPtr<UE::Sequencer::IOutlinerExtension>& StartNode, TSet<UE::Sequencer::TWeakViewModelPtr<UE::Sequencer::IOutlinerExtension>>& OutFilteredNodes)
{
	using namespace UE::Sequencer;

	for (TViewModelPtr<IOutlinerExtension> ChildNode : StartNode.AsModel()->GetDescendantsOfType<IOutlinerExtension>())
	{
		OutFilteredNodes.Add(ChildNode);
	}
}

static void AddParentNodes(const UE::Sequencer::TViewModelPtr<UE::Sequencer::IOutlinerExtension>& StartNode, TSet<UE::Sequencer::TWeakViewModelPtr<UE::Sequencer::IOutlinerExtension>>& OutFilteredNodes)
{
	using namespace UE::Sequencer;

	// Gather parent folders up the chain
	for (TViewModelPtr<IOutlinerExtension> ParentNode : StartNode.AsModel()->GetAncestorsOfType<IOutlinerExtension>())
	{
		OutFilteredNodes.Add(ParentNode);
	}
}

static bool PassesFilterStrings(FSequencer& Sequencer, const UE::Sequencer::TViewModelPtr<UE::Sequencer::IOutlinerExtension>& StartNode, const TArray<FString>& FilterStrings)
{
	using namespace UE::Sequencer;

	// If we have a filter string, make sure we match
	if (FilterStrings.Num() > 0)
	{
		TSharedPtr<FOutlinerViewModel> Outliner = Sequencer.GetViewModel()->GetOutliner();

		if (!Outliner)
		{
			return false;
		}

		FString DisplayLabel = StartNode->GetLabel().ToString();

		// check each string in the filter strings list against 
		for (const FString& String : FilterStrings)
		{
			if (!DisplayLabel.Contains(String))
			{
				return false;
			}
		}

		for (TViewModelPtr<IOutlinerExtension> Parent : StartNode.AsModel()->GetAncestorsOfType<IOutlinerExtension>())
		{
			if (!Parent->IsExpanded())
			{
				Parent->SetExpansion(true);
			}
		}
	}

	return true;
}

/**
 * Recursively filters nodes
 *
 * @param StartNode			The node to start from
 * @param Filters			The filter collection to test against
 * @param OutFilteredNodes	The list of all filtered nodes
 *
 * @return Whether this node passed filtering
  */

static bool FilterNodesRecursive(
	  FSequencer& Sequencer
	, const UE::Sequencer::TViewModelPtr<UE::Sequencer::IOutlinerExtension>& StartNode
	, TSharedPtr<FSequencerTrackFilterCollection> Filters, const TArray<FString>& FilterStrings
	, TSharedPtr<FSequencerTrackFilter_LevelFilter> LevelTrackFilter
	, TSet<UE::Sequencer::TWeakViewModelPtr<UE::Sequencer::IOutlinerExtension>>& OutFilteredNodes)
{
	using namespace UE::Sequencer;

	bool bAnyChildPassed = false;

	// Special case: If a parent node matches an active text search filter, the children should also pass the text search filter
	// so we stop filtering them based on text to eliminate special case conflicts with non-object binding nodes
	// with potential child object bindings nodes when only showing selected bindings. This is also faster.
	bool bPassedFilterStrings = PassesFilterStrings(Sequencer, StartNode, FilterStrings);
	const TArray<FString>& ChildFilterStrings = bPassedFilterStrings ? TArray<FString>() : FilterStrings;

	// Special case: Child nodes should always be processed, as they may force their parents to pass
	for (TViewModelPtr<IOutlinerExtension> Node : StartNode.AsModel()->GetChildrenOfType<IOutlinerExtension>())
	{
		if (FilterNodesRecursive(Sequencer, Node, Filters, ChildFilterStrings, LevelTrackFilter, OutFilteredNodes))
		{
			bAnyChildPassed = true;
		}
	}

	// After child nodes are processed, if this node didn't pass text filtering, fail it
	if (!bPassedFilterStrings)
	{
		return bAnyChildPassed;
	}

	// TODO: we can probably completely rewrite this logic by taking advantage of the new modular extensions.

	bool bPassedAnyFilters = false;
	bool bIsTrackOrObjectBinding = false;

	TSharedPtr<IPinnableExtension> Pinnable = StartNode.ImplicitCast();
	const bool bIsPinned = Pinnable && Pinnable->IsPinned();

	if (TSharedPtr<FTrackModel> TrackModel = StartNode.ImplicitCast())
	{
		bIsTrackOrObjectBinding = true;

		UMovieSceneTrack* Track = TrackModel->GetTrack();

		for (TSharedPtr<FChannelGroupModel> ChannelGroupModel : StartNode.AsModel()->GetDescendantsOfType<FChannelGroupModel>())
		{
			for (const TWeakViewModelPtr<FChannelModel>& WeakChannel : ChannelGroupModel->GetChannels())
			{
				if (TViewModelPtr<FChannelModel> ChannelModel = WeakChannel.Pin())
				{
					const TSharedPtr<IKeyArea>& KeyArea = ChannelModel->GetKeyArea();
					FMovieSceneChannel* Channel = KeyArea->ResolveChannel();
					if (Channel)
					{
						if (Filters->Num() == 0 || Filters->PassesAnyFilters(Channel))
						{
							bPassedAnyFilters = true;
							break;
						}
					}
				}
			}
		}
	
		if (bPassedAnyFilters || Filters->Num() == 0 || Filters->PassesAnyFilters(Track, TrackModel->GetLabel()))
		{
			bPassedAnyFilters = true;

			// Track nodes do not belong to a level, but might be a child of an objectbinding node that does
			if (LevelTrackFilter->IsActive())
			{
				TSharedPtr<IObjectBindingExtension> ObjectNode = StartNode.AsModel()->FindAncestorOfType<IObjectBindingExtension>();
				if (ObjectNode)
				{
					// The track belongs to an objectbinding node, start by assuming it doesn't match the level filter
					bPassedAnyFilters = false;

					for (TWeakObjectPtr<>& Object : Sequencer.FindObjectsInCurrentSequence(ObjectNode->GetObjectGuid()))
					{
						if (Object.IsValid() && LevelTrackFilter->PassesFilter(Object.Get()))
						{
							// If at least one of the objects on the objectbinding node pass the level filter, show the track
							bPassedAnyFilters = true;
							break;
						}
					}
				}
			}

			if (bPassedAnyFilters && Sequencer.GetSequencerSettings()->GetShowSelectedNodesOnly())
			{
				TSharedPtr<IObjectBindingExtension> ObjectNode = StartNode.AsModel()->FindAncestorOfType<IObjectBindingExtension>();
				// Always show pinned items
				if (!bIsPinned && ObjectNode)
				{
					const FMovieSceneBinding* Binding = Sequencer.GetFocusedMovieSceneSequence()->GetMovieScene()->FindBinding(ObjectNode->GetObjectGuid());
					if (!(Binding && Sequencer.IsBindingVisible(*Binding)))
					{
						return bAnyChildPassed;
					}
				}
			}
		}
	}
	else if (TViewModelPtr<FObjectBindingModel> ObjectNode = StartNode.ImplicitCast())
	{
		bIsTrackOrObjectBinding = true;

		for (TWeakObjectPtr<>& Object : Sequencer.FindObjectsInCurrentSequence(ObjectNode->GetObjectGuid()))
		{
			if (Object.IsValid() && (Filters->Num() == 0 || Filters->PassesAnyFilters(Object.Get(), StartNode->GetLabel()))
				&& LevelTrackFilter->PassesFilter(Object.Get()))
			{
				bPassedAnyFilters = true;
				break;
			}
		}

		if (bPassedAnyFilters && Sequencer.GetSequencerSettings()->GetShowSelectedNodesOnly() && !bIsPinned)
		{
			UMovieScene* MovieScene = Sequencer.GetFocusedMovieSceneSequence()->GetMovieScene();
			const FMovieSceneBinding* Binding = Sequencer.GetFocusedMovieSceneSequence()->GetMovieScene()->FindBinding(ObjectNode->GetObjectGuid());
			if (Binding && !Sequencer.IsBindingVisible(*Binding))
			{
				return bAnyChildPassed;
			}
		}
	}
	else if (TViewModelPtr<FCategoryModel> CategoryModel = StartNode.ImplicitCast())
	{
		if (TSharedPtr<FTrackModel> TrackNode = CategoryModel->FindAncestorOfType<FTrackModel>())
		{
			UMovieSceneTrack* Track = TrackNode->GetTrack();
			if (Filters->Num() == 0 || Filters->PassesAnyFilters(Track, StartNode->GetLabel()))
			{
				bPassedAnyFilters = true;
			}
		}
		if (bPassedAnyFilters && Sequencer.GetSequencerSettings()->GetShowSelectedNodesOnly())
		{
			TSharedPtr<IObjectBindingExtension> ParentObjectNode = StartNode.AsModel()->FindAncestorOfType<IObjectBindingExtension>();
			// Always show pinned items
			if (!bIsPinned && ParentObjectNode)
			{
				const FMovieSceneBinding* Binding = Sequencer.GetFocusedMovieSceneSequence()->GetMovieScene()->FindBinding(ParentObjectNode->GetObjectGuid());
				if (!(Binding && Sequencer.IsBindingVisible(*Binding)))
				{
					return bAnyChildPassed;
				}
			}
		}
	}
	else if (TViewModelPtr<FCategoryGroupModel> CategoryGroupModel = StartNode.ImplicitCast())
	{
		if (TSharedPtr<FTrackModel> TrackNode = CategoryGroupModel->FindAncestorOfType<FTrackModel>())
		{
			UMovieSceneTrack* Track = TrackNode->GetTrack();
			if (Filters->Num() == 0 || Filters->PassesAnyFilters(Track, FText::FromName(CategoryGroupModel->GetCategoryName())))
			{
				bPassedAnyFilters = true;
			}
		}
		if (bPassedAnyFilters && Sequencer.GetSequencerSettings()->GetShowSelectedNodesOnly())
		{
			TSharedPtr<IObjectBindingExtension> ParentObjectNode = StartNode.AsModel()->FindAncestorOfType<IObjectBindingExtension>();
			// Always show pinned items
			if (!bIsPinned && ParentObjectNode)
			{
				const FMovieSceneBinding* Binding = Sequencer.GetFocusedMovieSceneSequence()->GetMovieScene()->FindBinding(ParentObjectNode->GetObjectGuid());
				if (!(Binding && Sequencer.IsBindingVisible(*Binding)))
				{
					return bAnyChildPassed;
				}
			}
		}
	}
	else if (TViewModelPtr<FChannelGroupModel> ChannelGroupModel = StartNode.ImplicitCast())
	{
		for (const TWeakViewModelPtr<FChannelModel>& WeakChannel : ChannelGroupModel->GetChannels())
		{
			if (TViewModelPtr<FChannelModel> ChannelModel = WeakChannel.Pin())
			{
				const TSharedPtr<IKeyArea>& KeyArea = ChannelModel->GetKeyArea();
				FMovieSceneChannel* Channel = KeyArea->ResolveChannel();
				if (Channel)
				{
					if (Filters->Num() == 0 || Filters->PassesAnyFilters(Channel))
					{
						bPassedAnyFilters = true;
						break;
					}
				}
			}
		}
		if (bPassedAnyFilters && Sequencer.GetSequencerSettings()->GetShowSelectedNodesOnly())
		{
			TSharedPtr<IObjectBindingExtension> ParentObjectNode = StartNode.AsModel()->FindAncestorOfType<IObjectBindingExtension>();
			// Always show pinned items
			if (!bIsPinned && ParentObjectNode)
			{
				const FMovieSceneBinding* Binding = Sequencer.GetFocusedMovieSceneSequence()->GetMovieScene()->FindBinding(ParentObjectNode->GetObjectGuid());
				if (!(Binding && Sequencer.IsBindingVisible(*Binding)))
				{
					bPassedAnyFilters = false;
				}
			}
		}
	}
	else if (TViewModelPtr<FFolderModel> FolderModel = StartNode.ImplicitCast())
	{
		// Special case: If we're pinned, then we should pass regardless
		if (bIsPinned)
		{
			bPassedAnyFilters = true;
		}

		// Special case: If we're only filtering on text search, include folders and key areas in the search
		if (!bPassedAnyFilters && Filters->Num() == 0 && FilterStrings.Num() > 0)
		{
			bPassedAnyFilters = true;

			// Special case: but don't include if only showing selected bindings and we don't have child that passed
			if (Sequencer.GetSequencerSettings()->GetShowSelectedNodesOnly() && !bAnyChildPassed)
			{
				bPassedAnyFilters = false;

				// Special case: unless we're the child of a node that is a selected binding
				TSharedPtr<IObjectBindingExtension> ParentObjectNode = StartNode.AsModel()->FindAncestorOfType<IObjectBindingExtension>();
				// Always show pinned items
				if (ParentObjectNode)
				{
					const FMovieSceneBinding* Binding = Sequencer.GetFocusedMovieSceneSequence()->GetMovieScene()->FindBinding(ParentObjectNode->GetObjectGuid());
					if (Binding && Sequencer.IsBindingVisible(*Binding))
					{
						bPassedAnyFilters = true;
					}
				}
			}
		}
	}

	if (bPassedAnyFilters)
	{
		// If filtering on selection set is enabled, we need to run another pass to verify we're in an enabled node group
		UMovieSceneNodeGroupCollection& NodeGroups = Sequencer.GetFocusedMovieSceneSequence()->GetMovieScene()->GetNodeGroups();
		if (NodeGroups.HasAnyActiveFilter())
		{
			bPassedAnyFilters = false;
			
			constexpr bool bIncludeThis = true;
			for (TViewModelPtr<IOutlinerExtension> Parent : StartNode.AsModel()->GetAncestorsOfType<IOutlinerExtension>(bIncludeThis))
			{
				// Special case: Pinned tracks should be visible whether in the node group or not
				if (bIsPinned)
				{
					bPassedAnyFilters = true;
					break;
				}

				for (const UMovieSceneNodeGroup* NodeGroup : NodeGroups)
				{
					if (NodeGroup->GetEnableFilter() && NodeGroup->ContainsNode(IOutlinerExtension::GetPathName(Parent.AsModel())))
					{
						bPassedAnyFilters = true;
						break;
					}
				}
			}
		}
	}

	if (bPassedAnyFilters)
	{
		OutFilteredNodes.Add(StartNode);
		AddParentNodes(StartNode, OutFilteredNodes);

		// Special case: When only showing selected bindings, and a non-object passes text filtering
		// don't add it's children, as they may be a binding that is not seleceted. Selected child nodes will add themselves.
		if (!(Sequencer.GetSequencerSettings()->GetShowSelectedNodesOnly() && FilterStrings.Num() > 0 && !bIsTrackOrObjectBinding))
		{
			AddChildNodes(StartNode, OutFilteredNodes);
		}

		return true;
	}

	return bAnyChildPassed;
}

bool FSequencerNodeTree::UpdateFilters()
{
	using namespace UE::Sequencer;

	if (!bFilterUpdateRequested)
	{
		return false;
	}

	TSet<TWeakViewModelPtr<IOutlinerExtension>> PreviousFilteredNodes(FilteredNodes);

	FilteredNodes.Empty();
	const bool bHasActiveFilter = HasActiveFilter();

	UObject* PlaybackContext = Sequencer.GetPlaybackContext();
	UWorld* World = PlaybackContext ? PlaybackContext->GetWorld() : nullptr;
	TrackFilterLevelFilter->UpdateWorld(World);

	if (bHasActiveFilter)
	{
		// Build a list of strings that must be matched
		TArray<FString> FilterStrings;

		// Remove whitespace from the front and back of the string
		FilterString.TrimStartAndEndInline();
		FilterString.ParseIntoArray(FilterStrings, TEXT(" "), true /*bCullEmpty*/);

		for (const TViewModelPtr<IOutlinerExtension>& Node : GetRootNodes())
		{
			// Recursively filter all nodes, matching them against the list of filter strings.  All filter strings must be matched
			FilterNodesRecursive(Sequencer, Node, TrackFilters, FilterStrings, TrackFilterLevelFilter, FilteredNodes);
		}
	}

	bFilteringOnNodeGroups = Sequencer.GetFocusedMovieSceneSequence()->GetMovieScene()->GetNodeGroups().HasAnyActiveFilter();
	bFilterUpdateRequested = false;

	// Always include the bottom spacer
	if (FSequenceModel* SequenceModel = RootNode->CastThis<FSequenceModel>())
	{
		FilteredNodes.Add(CastViewModelChecked<IOutlinerExtension>(SequenceModel->GetBottomSpacer()));
	}

	// Count the total number of display nodes, and update filtered state.
	DisplayNodeCount = 0;
	for (const TViewModelPtr<IOutlinerExtension>& Item : RootNode->GetDescendantsOfType<IOutlinerExtension>())
	{
		const bool bIsNodeFilteredIn = !bHasActiveFilter || FilteredNodes.Contains(Item);
		Item->SetFilteredOut(!bIsNodeFilteredIn);

		++DisplayNodeCount;
	}

	// Return whether the new list of FilteredNodes is different than the previous list
	return (PreviousFilteredNodes.Num() != FilteredNodes.Num() || !PreviousFilteredNodes.Includes(FilteredNodes));
}

void FSequencerNodeTree::CleanupMuteSolo(UMovieScene* MovieScene)
{
	// Remove mute/solo markers for any nodes that no longer exist
	if (!MovieScene->IsReadOnly())
	{
		for (auto It = MovieScene->GetSoloNodes().CreateIterator(); It; ++It)
		{
			if (!GetNodeAtPath(*It))
			{
				It.RemoveCurrent();
			}
		}

		for (auto It = MovieScene->GetMuteNodes().CreateIterator(); It; ++It)
		{
			if (!GetNodeAtPath(*It))
			{
				It.RemoveCurrent();
			}
		}
	}
}

int32 FSequencerNodeTree::GetTotalDisplayNodeCount() const 
{ 
	// Subtract 1 for the spacer node which is always added
	return DisplayNodeCount - 1; 
}

int32 FSequencerNodeTree::GetFilteredDisplayNodeCount() const 
{ 
	// Subtract 1 for the spacer node which is always added
	return FilteredNodes.Num() - 1;
}

void FSequencerNodeTree::FilterNodes(const FString& InFilter)
{
	if (InFilter != FilterString)
	{
		FilterString = InFilter;
		bFilterUpdateRequested = true;
	}
}

void FSequencerNodeTree::NodeGroupsCollectionChanged()
{
	if (Sequencer.GetFocusedMovieSceneSequence()->GetMovieScene()->GetNodeGroups().HasAnyActiveFilter() || bFilteringOnNodeGroups)
	{
		RequestFilterUpdate();
	}
}

void FSequencerNodeTree::GetAllNodes(TArray<TSharedPtr<UE::Sequencer::FViewModel>>& OutNodes) const
{
	using namespace UE::Sequencer;

	const bool bIncludeRootNode = false;
	for (const FViewModelPtr& It : RootNode->GetDescendants(bIncludeRootNode))
	{
		OutNodes.Add(It.AsModel());
	}
}

void FSequencerNodeTree::GetAllNodes(TArray<TSharedRef<UE::Sequencer::FViewModel>>& OutNodes) const
{
	using namespace UE::Sequencer;

	const bool bIncludeRootNode = false;
	for (const FViewModelPtr& It : RootNode->GetDescendants(bIncludeRootNode))
	{
		OutNodes.Add(It.AsModel().ToSharedRef());
	}
}

