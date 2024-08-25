// Copyright Epic Games, Inc. All Rights Reserved.

#include "TrackEditors/BindingLifetimeTrackEditor.h"

#include "Containers/UnrealString.h"
#include "Delegates/Delegate.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "HAL/Platform.h"
#include "ISequencer.h"
#include "Internationalization/Internationalization.h"
#include "Misc/Guid.h"
#include "MovieScene.h"
#include "MovieSceneSequence.h"
#include "MovieSceneTrack.h"
#include "ScopedTransaction.h"
#include "Templates/Casts.h"
#include "Textures/SlateIcon.h"
#include "Tracks/MovieSceneBindingLifetimeTrack.h"
#include "Sections/MovieSceneBindingLifetimeSection.h"
#include "UObject/Class.h"
#include "UObject/UnrealNames.h"
#include "MovieSceneTrackEditor.h"
#include "Sections/BindingLifetimeSection.h"
#include "Widgets/SBoxPanel.h"
#include "MVVM/Views/ViewUtilities.h"

class ISequencerTrackEditor;


#define LOCTEXT_NAMESPACE "FBindingLifetimeTrackEditor"


TSharedRef<ISequencerTrackEditor> FBindingLifetimeTrackEditor::CreateTrackEditor(TSharedRef<ISequencer> InSequencer)
{
	return MakeShareable(new FBindingLifetimeTrackEditor(InSequencer));
}


FBindingLifetimeTrackEditor::FBindingLifetimeTrackEditor(TSharedRef<ISequencer> InSequencer)
	: FMovieSceneTrackEditor(InSequencer)
{ }

TSharedRef<ISequencerSection> FBindingLifetimeTrackEditor::MakeSectionInterface(UMovieSceneSection & SectionObject, UMovieSceneTrack & Track, FGuid ObjectBinding)
{
	return MakeShared<FBindingLifetimeSection>(SectionObject, GetSequencer());
}

void FBindingLifetimeTrackEditor::CreateNewSection(UMovieSceneTrack* Track, bool bSelect)
{
	TSharedPtr<ISequencer> SequencerPtr = GetSequencer();
	if (SequencerPtr.IsValid())
	{
		UMovieScene* FocusedMovieScene = GetFocusedMovieScene();
		FQualifiedFrameTime CurrentTime = SequencerPtr->GetLocalTime();

		FScopedTransaction Transaction(LOCTEXT("CreateNewSectionTransactionText", "Add Section"));


		UMovieSceneSection* NewSection = NewObject<UMovieSceneSection>(Track, UMovieSceneBindingLifetimeSection::StaticClass(), NAME_None, RF_Transactional);
		check(NewSection);

		Track->Modify();

		// first section by default should be infinite
		if (Track->GetAllSections().Num() == 0)
		{
			NewSection->SetRange(TRange<FFrameNumber>::All());
		}
		else
		{
			int32 Duration = 0;

			if (CurrentTime.Time.FrameNumber < FocusedMovieScene->GetPlaybackRange().GetUpperBoundValue())
			{
				Duration = FocusedMovieScene->GetPlaybackRange().GetUpperBoundValue().Value - CurrentTime.Time.FrameNumber.Value;
			}
			else
			{
				const float DefaultLengthInSeconds = 5.f;
				Duration = (DefaultLengthInSeconds * SequencerPtr->GetFocusedTickResolution()).FloorToFrame().Value;
			}

			NewSection->InitialPlacement(Track->GetAllSections(), CurrentTime.Time.FrameNumber.Value, Duration, false);
		}

		Track->AddSection(*NewSection);
		Track->UpdateEasing();

		if (bSelect)
		{
			SequencerPtr->EmptySelection();
			SequencerPtr->SelectSection(NewSection);
			SequencerPtr->ThrobSectionSelection();
		}

		SequencerPtr->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemAdded);
	}
}


UMovieSceneTrack* FBindingLifetimeTrackEditor::AddTrack(UMovieScene* FocusedMovieScene, const FGuid& ObjectHandle, TSubclassOf<UMovieSceneTrack> TrackClass, FName UniqueTypeName)
{
	UMovieSceneTrack* NewTrack = FMovieSceneTrackEditor::AddTrack(FocusedMovieScene, ObjectHandle, TrackClass, UniqueTypeName);

	if (auto* BindingLifetimeTrack = Cast<UMovieSceneBindingLifetimeTrack>(NewTrack))
	{
		BindingLifetimeTrack->Modify();
		CreateNewSection(BindingLifetimeTrack, false);
	}

	return NewTrack;
}


void FBindingLifetimeTrackEditor::BuildObjectBindingTrackMenu(FMenuBuilder& MenuBuilder, const TArray<FGuid>& ObjectBindings, const UClass* ObjectClass)
{
	UMovieSceneSequence* MovieSequence = GetSequencer()->GetFocusedMovieSceneSequence();

	// TODO: Do we want to restrict this to level sequences or no?
	if (!MovieSequence || MovieSequence->GetClass()->GetName() != TEXT("LevelSequence"))
	{
		return;
	}

	MenuBuilder.AddMenuEntry(
		LOCTEXT("AddBindingLifetimeTrack", "Binding Lifetime"),
		LOCTEXT("AddBindingLifetimeTrackTooltip", "Adds a new track that controls the lifetime of the track's object binding."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateRaw(this, &FBindingLifetimeTrackEditor::HandleAddBindingLifetimeTrackMenuEntryExecute, ObjectBindings),
			FCanExecuteAction::CreateSP(this, &FBindingLifetimeTrackEditor::CanAddBindingLifetimeTrack, ObjectBindings[0])
		)
	);
}


bool FBindingLifetimeTrackEditor::SupportsType(TSubclassOf<UMovieSceneTrack> Type) const
{
	return (Type == UMovieSceneBindingLifetimeTrack::StaticClass());
}


bool FBindingLifetimeTrackEditor::SupportsSequence(UMovieSceneSequence* InSequence) const
{
	ETrackSupport TrackSupported = InSequence ? InSequence->IsTrackSupported(UMovieSceneBindingLifetimeTrack::StaticClass()) : ETrackSupport::NotSupported;
	return TrackSupported == ETrackSupport::Supported;
}


void FBindingLifetimeTrackEditor::HandleAddBindingLifetimeTrackMenuEntryExecute(TArray<FGuid> ObjectBindings)
{
	FScopedTransaction AddSpawnTrackTransaction(LOCTEXT("AddBindingLifetimeTrack_Transaction", "Add Binding Lifetime Track"));

	for (FGuid ObjectBinding : ObjectBindings)
	{
		AddTrack(GetSequencer()->GetFocusedMovieSceneSequence()->GetMovieScene(), ObjectBinding, UMovieSceneBindingLifetimeTrack::StaticClass(), NAME_None);
	}
	GetSequencer()->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemAdded);
}


bool FBindingLifetimeTrackEditor::CanAddBindingLifetimeTrack(FGuid ObjectBinding) const
{
	return !GetSequencer()->GetFocusedMovieSceneSequence()->GetMovieScene()->FindTrack<UMovieSceneBindingLifetimeTrack>(ObjectBinding);
}

TSharedPtr<SWidget> FBindingLifetimeTrackEditor::BuildOutlinerEditWidget(const FGuid& ObjectBinding, UMovieSceneTrack* Track, const FBuildEditWidgetParams& Params)
{
	check(Track);

	TSharedPtr<ISequencer> SequencerPtr = GetSequencer();
	if (!SequencerPtr.IsValid())
	{
		return SNullWidget::NullWidget;
	}

	auto OnClickedCallback = [this, Track]() -> FReply
	{
		CreateNewSection(Track, true);
		return FReply::Handled();
	};

	return UE::Sequencer::MakeAddButton(LOCTEXT("AddSection", "Section"), FOnClicked::CreateLambda(OnClickedCallback), Params.ViewModel);
}


#undef LOCTEXT_NAMESPACE
