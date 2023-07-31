// Copyright Epic Games, Inc. All Rights Reserved.

#include "SequencerPlaylistItem_Sequence.h"
#include "SequencerPlaylist.h"
#include "SequencerPlaylistsLog.h"

#include "ISequencer.h"
#include "Framework/Notifications/NotificationManager.h"
#include "LevelSequence.h"
#include "MovieSceneFolder.h"
#include "MovieSceneTimeHelpers.h"
#include "Sections/MovieSceneSubSection.h"
#include "TrackEditors/SubTrackEditorBase.h" // for FSubTrackEditorUtil::CanAddSubSequence
#include "Tracks/MovieSceneSubTrack.h"
#include "Widgets/Notifications/SNotificationList.h"


#define LOCTEXT_NAMESPACE "SequencerPlaylists"


FText USequencerPlaylistItem_Sequence::GetDisplayName()
{
	return Sequence ? Sequence->GetDisplayName() : LOCTEXT("SequenceItemNullDisplayName", "(No sequence)");
}


FSequencerPlaylistItemPlayer_Sequence::FSequencerPlaylistItemPlayer_Sequence(TSharedRef<ISequencer> Sequencer)
	: WeakSequencer(Sequencer)
{
	Sequencer->OnStopEvent().AddRaw(this, &FSequencerPlaylistItemPlayer_Sequence::ClearItemStates);
}


FSequencerPlaylistItemPlayer_Sequence::~FSequencerPlaylistItemPlayer_Sequence()
{
	if (TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin())
	{
		Sequencer->OnStopEvent().RemoveAll(this);
	}
}


bool FSequencerPlaylistItemPlayer_Sequence::Play(USequencerPlaylistItem* Item)
{
	USequencerPlaylistItem_Sequence* SequenceItem = CastChecked<USequencerPlaylistItem_Sequence>(Item);
	if (!SequenceItem || !SequenceItem->Sequence)
	{
		return false;
	}

	TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
	if (!ensure(Sequencer))
	{
		return false;
	}

	ULevelSequence* RootSequence = Cast<ULevelSequence>(Sequencer->GetRootMovieSceneSequence());
	UMovieScene* RootScene = RootSequence->GetMovieScene();

	if (!FSubTrackEditorUtil::CanAddSubSequence(RootSequence, *SequenceItem->Sequence))
	{
		FNotificationInfo Info(FText::Format(LOCTEXT("InvalidSequence", "Invalid level sequence {0}. There could be a circular dependency."), SequenceItem->Sequence->GetDisplayName()));
		Info.bUseLargeFont = false;
		FSlateNotificationManager::Get().AddNotification(Info);
		return false;
	}

	RootSequence->Modify();

	FItemState& ItemState = ItemStates.FindOrAdd(Item);
	UMovieSceneSubTrack* WorkingTrack = GetOrCreateWorkingTrack(Item);

	if (UMovieSceneSubSection* HoldSection = ItemState.WeakHoldSection.Get())
	{
		EndSection(HoldSection);
		ItemState.WeakHoldSection.Reset();
	}

	UMovieScene* PlayScene = SequenceItem->Sequence->GetMovieScene();
	TRange<FFrameNumber> PlayRange = PlayScene->GetPlaybackRange();

	const FFrameTime StartFrameOffset_Ticks = ConvertFrameTime(FFrameNumber(SequenceItem->StartFrameOffset),
		PlayScene->GetDisplayRate(), PlayScene->GetTickResolution());

	const FFrameTime EndFrameOffset_Ticks = ConvertFrameTime(FFrameNumber(SequenceItem->EndFrameOffset),
		PlayScene->GetDisplayRate(), PlayScene->GetTickResolution());

	if (SequenceItem->StartFrameOffset > 0)
	{
		PlayRange.SetLowerBoundValue(PlayRange.GetLowerBoundValue() + StartFrameOffset_Ticks.FloorToFrame());
	}

	if (SequenceItem->EndFrameOffset > 0)
	{
		PlayRange.SetUpperBoundValue(PlayRange.GetUpperBoundValue() - EndFrameOffset_Ticks.FloorToFrame());
	}

	const float TimeScale = FMath::Max(SMALL_NUMBER, SequenceItem->PlaybackSpeed);
	const FFrameTime SingleLoopDuration = ConvertFrameTime(PlayRange.Size<FFrameTime>() / TimeScale,
		PlayScene->GetTickResolution(), RootScene->GetTickResolution());

	const FQualifiedFrameTime GlobalTime = Sequencer->GetGlobalTime();
	const FFrameNumber StartFrame = GlobalTime.Time.FloorToFrame();
	const int32 MinDuration = 1;
	const int32 MaxDuration = TNumericLimits<int32>::Max() - StartFrame.Value - 1;
	const int32 Duration = FMath::Clamp(
		(SingleLoopDuration * FMath::Max(1, SequenceItem->NumLoops + 1)).FloorToFrame().Value,
		MinDuration, MaxDuration);

	ItemState.PlayingUntil_RootTicks = FMath::Max(StartFrame.Value + Duration, ItemState.PlayingUntil_RootTicks);

	UMovieSceneSubSection* WorkingSubSection = WorkingTrack->AddSequence(SequenceItem->Sequence, StartFrame, Duration);

	WorkingSubSection->Parameters.TimeScale = TimeScale;

	if (SequenceItem->StartFrameOffset > 0)
	{
		WorkingSubSection->Parameters.StartFrameOffset = StartFrameOffset_Ticks.FloorToFrame();
	}

	if (SequenceItem->EndFrameOffset > 0)
	{
		WorkingSubSection->Parameters.EndFrameOffset = EndFrameOffset_Ticks.FloorToFrame();
	}

	if (SequenceItem->NumLoops != 0)
	{
		WorkingSubSection->Parameters.bCanLoop = true;
	}

	ItemState.WeakPlaySections.Add(WorkingSubSection);

	return true;
}


bool FSequencerPlaylistItemPlayer_Sequence::Stop(USequencerPlaylistItem* Item)
{
	bool bSequenceWasModified = false;

	USequencerPlaylistItem_Sequence* SequenceItem = CastChecked<USequencerPlaylistItem_Sequence>(Item);
	if (!SequenceItem || !SequenceItem->Sequence)
	{
		return bSequenceWasModified;
	}

	FItemState& ItemState = ItemStates.FindOrAdd(Item);
	UMovieSceneSubSection* HoldSection = ItemState.WeakHoldSection.Get();

	if (ItemState.WeakPlaySections.Num() == 0 && HoldSection == nullptr)
	{
		return bSequenceWasModified;
	}

	if (HoldSection)
	{
		bSequenceWasModified |= EndSection(HoldSection);
		ItemState.WeakHoldSection.Reset();
	}

	for (const TWeakObjectPtr<UMovieSceneSubSection>& WeakPlaySection : ItemState.WeakPlaySections)
	{
		if (UMovieSceneSubSection* PlaySection = WeakPlaySection.Get())
		{
			bSequenceWasModified |= EndSection(PlaySection);
		}
	}

	ItemState.WeakPlaySections.Empty();

	if (TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin())
	{
		const FFrameNumber Now = Sequencer->GetGlobalTime().Time.FloorToFrame();
		ItemState.PlayingUntil_RootTicks = FMath::Min(Now.Value, ItemState.PlayingUntil_RootTicks);
	}

	return bSequenceWasModified;
}


bool FSequencerPlaylistItemPlayer_Sequence::AddHold(USequencerPlaylistItem* Item)
{
	USequencerPlaylistItem_Sequence* SequenceItem = CastChecked<USequencerPlaylistItem_Sequence>(Item);
	if (!SequenceItem || !SequenceItem->Sequence)
	{
		return false;
	}

	TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
	if (!ensure(Sequencer))
	{
		return false;
	}

	FItemState& ItemState = ItemStates.FindOrAdd(Item);
	UMovieSceneSubSection* HoldSection = ItemState.WeakHoldSection.Get();
	if (HoldSection)
	{
		return false;
	}

	ULevelSequence* RootSequence = Cast<ULevelSequence>(Sequencer->GetRootMovieSceneSequence());
	UMovieScene* RootScene = RootSequence->GetMovieScene();

	if (!FSubTrackEditorUtil::CanAddSubSequence(RootSequence, *SequenceItem->Sequence))
	{
		FNotificationInfo Info(FText::Format(LOCTEXT("InvalidSequence", "Invalid level sequence {0}. There could be a circular dependency."), SequenceItem->Sequence->GetDisplayName()));
		Info.bUseLargeFont = false;
		FSlateNotificationManager::Get().AddNotification(Info);
		return false;
	}

	RootSequence->Modify();

	UMovieSceneSubTrack* WorkingTrack = GetOrCreateWorkingTrack(Item);

	ULevelSequence* HoldSequence = SequenceItem->Sequence;
	UMovieScene* HoldScene = HoldSequence->GetMovieScene();

	const FQualifiedFrameTime GlobalTime = Sequencer->GetGlobalTime();
	const FFrameNumber StartFrame = GlobalTime.Time.FloorToFrame();
	const int32 MaxDuration = TNumericLimits<int32>::Max() - StartFrame.Value - 1;
	HoldSection = WorkingTrack->AddSequence(HoldSequence, StartFrame, MaxDuration);
	ItemState.PlayingUntil_RootTicks = TNumericLimits<int32>::Max();

	if (SequenceItem->StartFrameOffset > 0)
	{
		const FFrameTime StartFrameOffset_Ticks = ConvertFrameTime(
			FFrameNumber(SequenceItem->StartFrameOffset),
			HoldScene->GetDisplayRate(),
			HoldScene->GetTickResolution());
		HoldSection->Parameters.StartFrameOffset = StartFrameOffset_Ticks.FloorToFrame();
	}

	HoldSection->Parameters.TimeScale = 0.f;

	ItemState.WeakHoldSection = HoldSection;

	return true;
}


bool FSequencerPlaylistItemPlayer_Sequence::Reset(USequencerPlaylistItem* Item)
{
	if (Item->bHoldAtFirstFrame)
	{
		Stop(Item);
		return AddHold(Item);
	}
	else
	{
		return Stop(Item);
	}
}


bool FSequencerPlaylistItemPlayer_Sequence::IsPlaying(USequencerPlaylistItem* Item) const
{
	USequencerPlaylistItem_Sequence* SequenceItem = CastChecked<USequencerPlaylistItem_Sequence>(Item);
	if (!SequenceItem || !SequenceItem->Sequence)
	{
		return false;
	}

	TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
	if (!ensure(Sequencer))
	{
		return false;
	}

	const FItemState* ItemState = ItemStates.Find(Item);
	if (!ItemState)
	{
		return false;
	}

	return ItemState->PlayingUntil_RootTicks > Sequencer->GetGlobalTime().Time.FloorToFrame().Value;
}


void FSequencerPlaylistItemPlayer_Sequence::ClearItemStates()
{
	ItemStates.Empty();
}


UMovieSceneSubTrack* FSequencerPlaylistItemPlayer_Sequence::GetOrCreateWorkingTrack(USequencerPlaylistItem* Item)
{
	USequencerPlaylistItem_Sequence* SequenceItem = CastChecked<USequencerPlaylistItem_Sequence>(Item);
	if (!SequenceItem || !SequenceItem->Sequence)
	{
		return nullptr;
	}

	TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
	if (!ensure(Sequencer))
	{
		return nullptr;
	}

	FItemState& ItemState = ItemStates.FindOrAdd(Item);

	ULevelSequence* RootSequence = Cast<ULevelSequence>(Sequencer->GetRootMovieSceneSequence());
	UMovieScene* RootScene = RootSequence->GetMovieScene();

	// Ensure we have a working track, and that it belongs to the current root sequence.
	if (ItemState.WeakTrack.IsValid() && ItemState.WeakTrack->GetTypedOuter<ULevelSequence>() == RootSequence)
	{
		return ItemState.WeakTrack.Get();
	}

	UMovieSceneSubTrack* NewWorkingTrack = RootScene->AddMasterTrack<UMovieSceneSubTrack>();
	NewWorkingTrack->SetDisplayName(FText::Format(LOCTEXT("SequenceItemTrackName", "Item - {0}"), SequenceItem->Sequence->GetDisplayName()));

	// Find or create folder named for our playlist, and organize our track beneath it.
	FText PlaylistName = FText::GetEmpty();
	if (USequencerPlaylist* OuterPlaylist = SequenceItem->GetTypedOuter<USequencerPlaylist>())
	{
		PlaylistName = FText::FromString(*OuterPlaylist->GetName());
	}

	const FText PlaylistFolderNameText = FText::Format(LOCTEXT("PlaylistFolderName", "Playlist - {0}"), PlaylistName);
	const FName PlaylistFolderName = FName(*PlaylistFolderNameText.ToString());

	UMovieSceneFolder* FolderToUse = nullptr;
	for (UMovieSceneFolder* Folder : RootScene->GetRootFolders())
	{
		if (Folder->GetFolderName() == PlaylistFolderName)
		{
			FolderToUse = Folder;
			break;
		}
	}

	if (FolderToUse == nullptr)
	{
		FolderToUse = NewObject<UMovieSceneFolder>(RootScene, NAME_None, RF_Transactional);
		FolderToUse->SetFolderName(PlaylistFolderName);
		//RootScene->GetRootFolders().Add(FolderToUse);
	}

	FolderToUse->AddChildMasterTrack(NewWorkingTrack);

	ItemState.WeakTrack = NewWorkingTrack;
	return NewWorkingTrack;
}


bool FSequencerPlaylistItemPlayer_Sequence::EndSection(UMovieSceneSection* Section)
{
	bool bSequenceWasModified = false;

	if (!ensure(Section))
	{
		return bSequenceWasModified;
	}

	TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
	if (!ensure(Sequencer))
	{
		return bSequenceWasModified;
	}

	ULevelSequence* RootSequence = Cast<ULevelSequence>(Sequencer->GetRootMovieSceneSequence());

	UMovieScene* SectionScene = Section->GetTypedOuter<UMovieScene>();
	check(SectionScene);

	const FQualifiedFrameTime GlobalTime = Sequencer->GetGlobalTime();
	const FFrameTime SectionNow = GlobalTime.ConvertTo(SectionScene->GetTickResolution());
	if (Section->IsTimeWithinSection(SectionNow.FloorToFrame()))
	{
		RootSequence->Modify();
		bSequenceWasModified = true;

		Section->SetEndFrame(TRangeBound<FFrameNumber>::Exclusive(SectionNow.FloorToFrame()));

		// Remove degenerate sections. This can happen if we reset then stop a held item while paused.
		if (Section->GetRange().IsEmpty())
		{
			if (UMovieSceneTrack* SectionTrack = Section->GetTypedOuter<UMovieSceneTrack>())
			{
				SectionTrack->Modify();
				SectionTrack->RemoveSection(*Section);
				return bSequenceWasModified;
			}
		}

		// Workaround for not being able to interrupt first playthrough if bCanLoop == true.
		if (UMovieSceneSubSection* SubSection = Cast<UMovieSceneSubSection>(Section))
		{
			if (SubSection->Parameters.bCanLoop)
			{
				// Calc whether we've looped; if not, set bCanLoop back to false.
				FFrameTime InnerTime;
				FMovieSceneWarpCounter WarpCounter;
				SubSection->OuterToInnerTransform().TransformTime(GlobalTime.Time, InnerTime, WarpCounter);
				const uint32 LastWarpCount = WarpCounter.LastWarpCount();
				if (LastWarpCount == 0)
				{
					SubSection->Parameters.bCanLoop = false;
				}
			}
		}
	}

	return bSequenceWasModified;
}


#undef LOCTEXT_NAMESPACE
