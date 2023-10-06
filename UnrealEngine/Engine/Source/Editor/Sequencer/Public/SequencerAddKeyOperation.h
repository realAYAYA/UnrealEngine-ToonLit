// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Containers/Map.h"
#include "Containers/Set.h"
#include "Containers/SparseArray.h"
#include "SequencerCoreFwd.h"
#include "Misc/FrameNumber.h"
#include "SequencerKeyParams.h"
#include "Templates/SharedPointer.h"

class IKeyArea;
class ISequencer;
class ISequencerSection;
class ISequencerTrackEditor;
class UMovieSceneTrack;
namespace UE::Sequencer { struct FKeyOperation; }
struct FFrameNumber;

namespace UE
{
namespace Sequencer
{

class FChannelGroupModel;
class FViewModel;
class ITrackExtension;
class IOutlinerExtension;

/**
 * Temporary structure used for consistent add-key behavior for a set of display nodes
 * Ultimately the operation will call ISequencerTrackEditor::ProcessKeyOperation for each track editor that needs to add keys.
 */
struct SEQUENCER_API FAddKeyOperation
{
	/**
	 * Construct an operation from any set of display nodes. Each node in the set will receive keys for all decendant key areas.
	 *
	 * @param InModels A set of all the models to key
	 */
	static FAddKeyOperation FromNodes(const TSet<TWeakViewModelPtr<IOutlinerExtension>>& InModels);


	/**
	 * Construct an operation from a single display node. Every key area underneath this node will receive keys.
	 */
	static FAddKeyOperation FromNode(TWeakPtr<FViewModel> InModel);


	/**
	 * Construct an operation from some key areas on a track.
	 */
	static FAddKeyOperation FromKeyAreas(ISequencerTrackEditor* TrackEditor, const TArrayView<TSharedRef<IKeyArea>> InKeyAreas);


	/**
	 * Commit this operation by choosing the section(s) to key for each key area, and adding a key at the specified time
	 *
	 * @param InKeyTime     The time to add keys at
	 * @param InSequencer   The sequencer instance that is performing this operations
	 */
	void Commit(FFrameNumber InKeyTime, ISequencer& InSequencer);

private:

	/**
	 * Add a set of nodes to this operation that have already had child nodes removed (ie only parent nodes should exist in the set)
	 *
	 * @param InNodes     A set of nodes to add to this operation that contains no child nodes
	 */
	void AddPreFilteredNodes(TArrayView<const TWeakPtr<FViewModel>> InNodes);


	/**
	 * Add any keyable areas to the list of potential things to key
	 *
	 * @param InTrackEditor         The track editor
	 * @param InKeyAnythingBeneath  A node to search within for key areas
	 */
	bool ConsiderKeyableAreas(TSharedPtr<ITrackExtension> InTrackModel, FViewModelPtr InKeyAnythingBeneath);


	/**
	 * Add key areas for a key area display node to this operation
	 *
	 * @param InTrackNode         The current track node
	 * @param InKeyAreaNode       The key area node to add key areas from
	 */
	bool ProcessKeyArea(TSharedPtr<ITrackExtension> InTrackModel, TViewModelPtr<FChannelGroupModel> InChannelGroupModel);


	/**
	 * Add a key area to this operation
	 *
	 * @param InTrackEditor       The track editor responsible for the key area
	 * @param InKeyArea           The key area to add
	 */
	bool ProcessKeyArea(ISequencerTrackEditor* InTrackEditor, TSharedPtr<IKeyArea> InKeyArea);


	/**
	 * Retrieve the operation that relates to a specific track editor instance
	 */
	FKeyOperation& GetTrackOperation(ISequencerTrackEditor* TrackEditor);

private:

	FAddKeyOperation() {}

	/** Map of key operations stored by their track editor. */
	TMap<ISequencerTrackEditor*, FKeyOperation> OperationsByTrackEditor;
};

} // namespace Sequencer
} // namespace UE

