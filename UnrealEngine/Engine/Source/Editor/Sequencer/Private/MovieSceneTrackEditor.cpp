// Copyright Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTrackEditor.h"
#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "Templates/SubclassOf.h"
#include "ISequencer.h"
#include "Framework/Commands/UICommandList.h"
#include "ScopedTransaction.h"
#include "MovieSceneTrack.h"
#include "ISequencerTrackEditor.h"
#include "MovieScene.h"
#include "MovieSceneSequence.h"
#include "Sequencer.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "SequencerUtilities.h"
#include "Framework/Application/SlateApplication.h"


FMovieSceneTrackEditor::FMovieSceneTrackEditor(TSharedRef<ISequencer> InSequencer)
	: Sequencer(InSequencer)
{ 
}

FMovieSceneTrackEditor::~FMovieSceneTrackEditor() 
{ 
}

UMovieSceneSequence* FMovieSceneTrackEditor::GetMovieSceneSequence() const
{
	return Sequencer.Pin()->GetFocusedMovieSceneSequence();
}

TOptional<FFrameNumber> FMovieSceneTrackEditor::NextKeyTime;
bool FMovieSceneTrackEditor::bKeying;

FFrameNumber FMovieSceneTrackEditor::GetTimeForKey()
{ 
	if (bKeying && NextKeyTime.IsSet())
	{
		return NextKeyTime.GetValue();
	}

	// Otherwise, key at the current time
	TSharedPtr<ISequencer> SequencerPin = Sequencer.Pin();
	return SequencerPin.IsValid() ? SequencerPin->GetLocalTime().Time.FrameNumber : FFrameNumber(0);
}

void FMovieSceneTrackEditor::BeginKeying(FFrameNumber InFrameNumber)
{
	bKeying = true;
	NextKeyTime = InFrameNumber;
}

void FMovieSceneTrackEditor::EndKeying()
{
	bKeying = false;
	NextKeyTime.Reset();
}

void FMovieSceneTrackEditor::UpdatePlaybackRange()
{
	TSharedPtr<ISequencer> SequencerPin = Sequencer.Pin();
	if( SequencerPin.IsValid()  )
	{
		SequencerPin->UpdatePlaybackRange();
	}
}

TSharedRef<ISequencerSection> FMovieSceneTrackEditor::MakeSectionInterface(UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding)
{
	return MakeShared<FSequencerSection>(SectionObject);
}

void FMovieSceneTrackEditor::AnimatablePropertyChanged( FOnKeyProperty OnKeyProperty )
{
	check(OnKeyProperty.IsBound());

	// Get the movie scene we want to autokey
	UMovieSceneSequence* MovieSceneSequence = GetMovieSceneSequence();
	if (MovieSceneSequence)
	{
		FFrameNumber KeyTime = GetTimeForKey();

		// @todo Sequencer - The sequencer probably should have taken care of this
		MovieSceneSequence->SetFlags(RF_Transactional);
	
		// Create a transaction record because we are about to add keys
		const bool bShouldActuallyTransact = !GIsTransacting;		// Don't transact if we're recording in a PIE world.  That type of keyframe capture cannot be undone.
		FScopedTransaction AutoKeyTransaction( NSLOCTEXT("AnimatablePropertyTool", "PropertyChanged", "Animatable Property Changed"), bShouldActuallyTransact );

		FKeyPropertyResult KeyPropertyResult = OnKeyProperty.Execute( KeyTime );

		// When shift is down, enable adding keys/sections one after the other
		const bool bInProperSlateThread = (IsInGameThread() || IsInSlateThread() || IsInAsyncLoadingThread());
		if (bInProperSlateThread && FSlateApplication::Get().GetModifierKeys().IsShiftDown())
		{
			for (TWeakObjectPtr<UMovieSceneSection> NewSection : KeyPropertyResult.SectionsCreated)
			{
				if (NewSection.IsValid())
				{
					NextKeyTime = NewSection.Get()->GetExclusiveEndFrame();
					break;
				}
			}
		}

		if (KeyPropertyResult.bTrackCreated)
		{
			// If a track is created evaluate immediately so that the pre-animated state can be stored
			Sequencer.Pin()->NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::RefreshAllImmediately );
		}
		else if (KeyPropertyResult.bTrackModified)
		{
			Sequencer.Pin()->NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::MovieSceneStructureItemAdded );
		}
		else if (KeyPropertyResult.bKeyCreated)
		{
			Sequencer.Pin()->NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::TrackValueChanged );
		}
		else
		{
			// If the only thing we changed as a result of the external change were channel defaults, we suppress automatic
			// re-evaluation of the sequence for this change to ensure that the object does not have the change immediately overwritten
			// by animated channels that have keys, but did not have keys added
			UMovieSceneSequence* FocusedSequence = Sequencer.Pin()->GetFocusedMovieSceneSequence();
			if (FocusedSequence)
			{
				Sequencer.Pin()->SuppressAutoEvaluation(FocusedSequence, FocusedSequence->GetSignature());
			}
		}

		UpdatePlaybackRange();
	}
}


FMovieSceneTrackEditor::FFindOrCreateHandleResult FMovieSceneTrackEditor::FindOrCreateHandleToObject( UObject* Object, bool bCreateHandleIfMissing, const FName& CreatedFolderName)
{
	FFindOrCreateHandleResult Result;
	bool bHandleWasValid = GetSequencer()->GetHandleToObject( Object, false ).IsValid();
	

	Result.Handle = GetSequencer()->GetHandleToObject( Object, bCreateHandleIfMissing, CreatedFolderName);
	Result.bWasCreated = bHandleWasValid == false && Result.Handle.IsValid();
	return Result;
}


FMovieSceneTrackEditor::FFindOrCreateTrackResult FMovieSceneTrackEditor::FindOrCreateTrackForObject( const FGuid& ObjectHandle, TSubclassOf<UMovieSceneTrack> TrackClass, FName PropertyName, bool bCreateTrackIfMissing )
{
	FFindOrCreateTrackResult Result;
	bool bTrackExisted;

	UMovieScene* MovieScene = GetSequencer()->GetFocusedMovieSceneSequence()->GetMovieScene();
	Result.Track = MovieScene->FindTrack( TrackClass, ObjectHandle, PropertyName );
	bTrackExisted = Result.Track != nullptr;

	if (!Result.Track && bCreateTrackIfMissing)
	{
		Result.Track = AddTrack(MovieScene, ObjectHandle, TrackClass, PropertyName);
	}

	Result.bWasCreated = bTrackExisted == false && Result.Track != nullptr;

	return Result;
}


const TSharedPtr<ISequencer> FMovieSceneTrackEditor::GetSequencer() const
{
	return Sequencer.Pin();
}

UMovieSceneTrack* FMovieSceneTrackEditor::AddTrack(UMovieScene* FocusedMovieScene, const FGuid& ObjectHandle, TSubclassOf<class UMovieSceneTrack> TrackClass, FName UniqueTypeName)
{
	return FocusedMovieScene->AddTrack(TrackClass, ObjectHandle);
}

void FMovieSceneTrackEditor::BindCommands(TSharedRef<FUICommandList> SequencerCommandBindings) 
{ 
}

void FMovieSceneTrackEditor::BuildAddTrackMenu(FMenuBuilder& MenuBuilder) 
{ 
}

void FMovieSceneTrackEditor::BuildObjectBindingEditButtons(TSharedPtr<SHorizontalBox> EditBox, const FGuid& ObjectBinding, const UClass* ObjectClass) 
{ 
}

void FMovieSceneTrackEditor::BuildObjectBindingTrackMenu(FMenuBuilder& MenuBuilder, const TArray<FGuid>& ObjectBindings, const UClass* ObjectClass)
{
}

TSharedPtr<SWidget> FMovieSceneTrackEditor::BuildOutlinerEditWidget(const FGuid& ObjectBinding, UMovieSceneTrack* Track, const FBuildEditWidgetParams& Params)
{
	if (Track && Track->GetSupportedBlendTypes().Num() > 0)
	{
		TWeakPtr<ISequencer> WeakSequencer = GetSequencer();

		const int32 RowIndex = Params.TrackInsertRowIndex;
		auto SubMenuCallback = [=]() -> TSharedRef<SWidget>
		{
			FMenuBuilder MenuBuilder(true, nullptr);

			FSequencerUtilities::PopulateMenu_CreateNewSection(MenuBuilder, RowIndex, Track, WeakSequencer);

			return MenuBuilder.MakeWidget();
		};

		return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			FSequencerUtilities::MakeAddButton(NSLOCTEXT("MovieSceneTrackEditor", "AddSection", "Section"), FOnGetContent::CreateLambda(SubMenuCallback), Params.NodeIsHovered, GetSequencer())
		];
	}
	else
	{
		return TSharedPtr<SWidget>(); 
	}
}

void FMovieSceneTrackEditor::BuildTrackContextMenu( FMenuBuilder& MenuBuilder, UMovieSceneTrack* Track ) 
{ 
}

bool FMovieSceneTrackEditor::HandleAssetAdded(UObject* Asset, const FGuid& TargetObjectGuid) 
{ 
	return false; 
}

bool FMovieSceneTrackEditor::OnAllowDrop(const FDragDropEvent& DragDropEvent, FSequencerDragDropParams& DragDropParams)
{
	return false;
}

FReply FMovieSceneTrackEditor::OnDrop(const FDragDropEvent& DragDropEvent, const FSequencerDragDropParams& DragDropParams)
{
	return FReply::Unhandled();
}

void FMovieSceneTrackEditor::OnInitialize() 
{ 
}

void FMovieSceneTrackEditor::OnRelease() 
{ 
}

void FMovieSceneTrackEditor::Tick(float DeltaTime) 
{ 
}

UMovieScene* FMovieSceneTrackEditor::GetFocusedMovieScene() const
{
	UMovieSceneSequence* FocusedSequence = GetSequencer()->GetFocusedMovieSceneSequence();
	return FocusedSequence->GetMovieScene();
}
