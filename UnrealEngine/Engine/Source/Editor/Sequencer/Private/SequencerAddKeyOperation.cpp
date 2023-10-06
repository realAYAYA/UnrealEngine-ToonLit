// Copyright Epic Games, Inc. All Rights Reserved.

#include "SequencerAddKeyOperation.h"

#include "Containers/Array.h"
#include "HAL/PlatformCrt.h"
#include "IKeyArea.h"
#include "ISequencer.h"
#include "ISequencerSection.h"
#include "ISequencerTrackEditor.h"
#include "MVVM/Extensions/IOutlinerExtension.h"
#include "MVVM/Extensions/ITrackExtension.h"
#include "MVVM/ViewModelTypeID.h"
#include "MVVM/ViewModels/ChannelModel.h"
#include "MVVM/ViewModels/ViewModel.h"
#include "MVVM/ViewModels/ViewModelIterators.h"
#include "Misc/AssertionMacros.h"
#include "Misc/FrameNumber.h"
#include "MovieSceneSection.h"
#include "MovieSceneTrack.h"
#include "SequencerKeyParams.h"
#include "Templates/Tuple.h"
#include "Templates/TypeHash.h"
#include "Templates/UnrealTemplate.h"

namespace UE::Sequencer { class FSequenceModel; }


namespace UE
{
namespace Sequencer
{

FAddKeyOperation FAddKeyOperation::FromNodes(const TSet<TWeakViewModelPtr<IOutlinerExtension>>& InNodes)
{
	FAddKeyOperation Operation;

	TArray<TWeakPtr<FViewModel>> FilteredNodes;

	// Remove any child nodes that have a parent also included in the set
	for (const TWeakViewModelPtr<IOutlinerExtension>& ProspectiveNode : InNodes)
	{
		TSharedPtr<FViewModel> Parent = ProspectiveNode.Pin().AsModel()->GetParent();
		while (Parent)
		{
			if (InNodes.Contains(CastViewModel<IOutlinerExtension>(Parent)))
			{
				goto Continue;
			}
			Parent = Parent->GetParent();
		}

		FilteredNodes.Add(ProspectiveNode);

	Continue:
		continue;
	}

	Operation.AddPreFilteredNodes(FilteredNodes);
	return Operation;
}

FAddKeyOperation FAddKeyOperation::FromNode(TWeakPtr<FViewModel> InNode)
{
	FAddKeyOperation Operation;
	Operation.AddPreFilteredNodes(MakeArrayView(&InNode, 1));
	return Operation;
}

FAddKeyOperation FAddKeyOperation::FromKeyAreas(ISequencerTrackEditor* TrackEditor, const TArrayView<TSharedRef<IKeyArea>> InKeyAreas)
{
	FAddKeyOperation Operation;
	if (ensure(TrackEditor))
	{
		for (const TSharedRef<IKeyArea>& KeyArea : InKeyAreas)
		{
			Operation.ProcessKeyArea(TrackEditor, KeyArea);
		}
	}
	return Operation;
}

void FAddKeyOperation::AddPreFilteredNodes(TArrayView<const TWeakPtr<FViewModel>> FilteredNodes)
{
	TSharedPtr<FSequenceModel> SequenceModel;
	const TArray<FViewModelTypeID> TypeIDs({ ITrackExtension::ID, IOutlinerExtension::ID });
	for (const TWeakPtr<FViewModel>& FilteredNode : FilteredNodes)
	{
		TSharedPtr<FViewModel> Node = FilteredNode.Pin();
		TSharedPtr<FViewModel> ParentModel = Node->FindAncestorOfTypes(MakeArrayView(TypeIDs));
		if (ParentModel)
		{
			ConsiderKeyableAreas(ParentModel->CastThisShared<ITrackExtension>(), Node);
		}
		else
		{
			constexpr bool bIncludeThis = true;
			for (FParentFirstChildIterator Child(FilteredNode.Pin(), bIncludeThis); Child; ++Child)
			{
				if (Child->IsA<ITrackExtension>() && Child->IsA<IOutlinerExtension>())
				{
					TSharedPtr<ITrackExtension> TrackExtension = Child->CastThisShared<ITrackExtension>();
					ConsiderKeyableAreas(TrackExtension, *Child);
				}
			}
		}
	}
}

bool FAddKeyOperation::ConsiderKeyableAreas(TSharedPtr<ITrackExtension> InTrackModel, FViewModelPtr KeyAnythingBeneath)
{
	bool bKeyedAnything = false;

	constexpr bool bIncludeThis = true;
	
	// Prefer the section that is marked SectionToKey
	for (const TViewModelPtr<FSectionModel>& SectionModel : KeyAnythingBeneath->GetDescendantsOfType<FSectionModel>(bIncludeThis))
	{
		UMovieSceneSection* Section = SectionModel->GetSection();
		UMovieSceneTrack* Track = Cast<UMovieSceneTrack>(Section->GetOuter());
		if (Track && Track->GetSectionToKey() == Section)
		{
			for (TParentFirstChildIterator<FChannelGroupModel> ChannelGroupModelIt(SectionModel->AsShared(), bIncludeThis); ChannelGroupModelIt; ++ChannelGroupModelIt)
			{
				bKeyedAnything |= ProcessKeyArea(InTrackModel, *ChannelGroupModelIt);
			}
		}
	}

	// Otherwise if nothing was found, key all
	if (!bKeyedAnything)
	{
		for (TParentFirstChildIterator<FChannelGroupModel> ChannelGroupModelIt(KeyAnythingBeneath->AsShared(), bIncludeThis); ChannelGroupModelIt; ++ChannelGroupModelIt)
		{
			bKeyedAnything |= ProcessKeyArea(InTrackModel, *ChannelGroupModelIt);
		}
	}

	return bKeyedAnything;
}

bool FAddKeyOperation::ProcessKeyArea(TSharedPtr<ITrackExtension> InTrackModel, TViewModelPtr<FChannelGroupModel> InChannelGroupModel)
{
	bool bKeyedAnything = false;
	ISequencerTrackEditor* TrackEditor = InTrackModel->GetTrackEditor().Get();

	IOutlinerExtension* OutlinerExtension = InChannelGroupModel->CastThis<IOutlinerExtension>();
	if (!OutlinerExtension || OutlinerExtension->IsFilteredOut() == false)
	{
		constexpr bool bIncludeThis = true;
		for (const TWeakViewModelPtr<FChannelModel>& WeakChannel : InChannelGroupModel->GetChannels())
		{
			if (TSharedPtr<FChannelModel> Channel = WeakChannel.Pin())
			{
				bKeyedAnything |= ProcessKeyArea(TrackEditor, Channel->GetKeyArea());
			}
		}
	}

	return bKeyedAnything;
}

bool FAddKeyOperation::ProcessKeyArea(ISequencerTrackEditor* InTrackEditor, TSharedPtr<IKeyArea> InKeyArea)
{
	TSharedPtr<ISequencerSection> Section       = InKeyArea->GetSectionInterface();
	UMovieSceneSection*           SectionObject = Section       ? Section->GetSectionObject()                      : nullptr;
	UMovieSceneTrack*             TrackObject   = SectionObject ? SectionObject->GetTypedOuter<UMovieSceneTrack>() : nullptr;

	if (TrackObject)
	{
		GetTrackOperation(InTrackEditor).Populate(TrackObject, Section, InKeyArea);
		return true;
	}

	return false;
}

void FAddKeyOperation::Commit(FFrameNumber KeyTime, ISequencer& InSequencer)
{
	for (TTuple<ISequencerTrackEditor*, FKeyOperation>& Pair : OperationsByTrackEditor)
	{
		Pair.Value.InitializeOperation(KeyTime);
		Pair.Key->ProcessKeyOperation(KeyTime, Pair.Value, InSequencer);
	}

	InSequencer.UpdatePlaybackRange();
	InSequencer.NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::TrackValueChanged);
}

FKeyOperation& FAddKeyOperation::GetTrackOperation(ISequencerTrackEditor* TrackEditor)
{
	return OperationsByTrackEditor.FindOrAdd(TrackEditor);
}

} // namespace Sequencer
} // namespace UE

