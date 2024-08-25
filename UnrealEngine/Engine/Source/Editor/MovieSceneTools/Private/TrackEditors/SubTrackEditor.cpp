// Copyright Epic Games, Inc. All Rights Reserved.

#include "TrackEditors/SubTrackEditor.h"
#include "Rendering/DrawElements.h"
#include "Widgets/SBoxPanel.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "EngineGlobals.h"
#include "Engine/Engine.h"
#include "Modules/ModuleManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SBox.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Styling/AppStyle.h"
#include "GameFramework/PlayerController.h"
#include "Sections/MovieSceneSubSection.h"
#include "Tracks/MovieSceneSubTrack.h"
#include "Tracks/MovieSceneCinematicShotTrack.h"
#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"
#include "MVVM/Views/ViewUtilities.h"
#include "SequencerSectionPainter.h"
#include "TrackEditors/SubTrackEditorBase.h"
#include "DragAndDrop/AssetDragDropOp.h"
#include "MovieSceneMetaData.h"
#include "MovieSceneSequence.h"
#include "MovieSceneToolHelpers.h"
#include "MovieSceneToolsProjectSettings.h"
#include "Misc/QualifiedFrameTime.h"
#include "MovieSceneTimeHelpers.h"
#include "EngineAnalytics.h"
#include "Interfaces/IAnalyticsProvider.h"
#include "Algo/Accumulate.h"
#include "AssetToolsModule.h"
#include "Interfaces/IMainFrameModule.h"
#include "IDetailsView.h"
#include "IStructureDetailsView.h"
#include "PropertyEditorModule.h"

#define LOCTEXT_NAMESPACE "FSubTrackEditor"


/**
 * A generic implementation for displaying simple property sections.
 */
class FSubSection
	: public TSubSectionMixin<>
{
public:

	FSubSection(TSharedPtr<ISequencer> InSequencer, UMovieSceneSection& InSection, TSharedPtr<FSubTrackEditor> InSubTrackEditor)
		: TSubSectionMixin(InSequencer, *CastChecked<UMovieSceneSubSection>(&InSection))
		, SubTrackEditor(InSubTrackEditor)
	{
	}

public:

	// ISequencerSection interface

	virtual void BuildSectionContextMenu(FMenuBuilder& MenuBuilder, const FGuid& ObjectBinding) override
	{
		ISequencerSection::BuildSectionContextMenu(MenuBuilder, ObjectBinding);

		UMovieSceneSubSection* Section = &GetSubSectionObject();
		
		FString DisplayName = SubTrackEditor.Pin()->GetSubSectionDisplayName(Section);

		MenuBuilder.BeginSection(NAME_None, LOCTEXT("SequenceMenuText", "Sequence"));
		{
			MenuBuilder.AddSubMenu(
				LOCTEXT("TakesMenu", "Takes"),
				LOCTEXT("TakesMenuTooltip", "Subsequence takes"),
				FNewMenuDelegate::CreateLambda([this, Section](FMenuBuilder& InMenuBuilder) { SubTrackEditor.Pin()->AddTakesMenu(Section, InMenuBuilder); }));

			MenuBuilder.AddMenuEntry(
				LOCTEXT("NewTake", "New Take"),
				FText::Format(LOCTEXT("NewTakeTooltip", "Create a new take for {0}"), FText::FromString(DisplayName)),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateSP(SubTrackEditor.Pin().ToSharedRef(), &FSubTrackEditor::CreateNewTake, Section))
			);

			MenuBuilder.AddMenuEntry(
				LOCTEXT("InsertNewSequence", "Insert Sequence"),
				LOCTEXT("InsertNewSequenceTooltip", "Insert a new sequence at the current time"),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateSP(SubTrackEditor.Pin().ToSharedRef(), &FSubTrackEditor::InsertSection, Cast<UMovieSceneTrack>(Section->GetOuter())))
			);

			MenuBuilder.AddMenuEntry(
				LOCTEXT("DuplicateSequence", "Duplicate Sequence"),
				FText::Format(LOCTEXT("DuplicateSequenceTooltip", "Duplicate {0} to create a new sequence"), FText::FromString(DisplayName)),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateSP(SubTrackEditor.Pin().ToSharedRef(), &FSubTrackEditor::DuplicateSection, Section))
			);
		
			MenuBuilder.AddMenuEntry(
				LOCTEXT("EditMetaData", "Edit Meta Data"),
				LOCTEXT("EditMetaDataTooltip", "Edit meta data"),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateSP(SubTrackEditor.Pin().ToSharedRef(), &FSubTrackEditor::EditMetaData, Section))
			);

			MenuBuilder.AddMenuEntry(
				LOCTEXT("PlayableDirectly_Label", "Playable Directly"),
				LOCTEXT("PlayableDirectly_Tip", "When enabled, this sequence will also support being played directly outside of the root sequence. Disable this to save some memory on complex hierarchies of sequences."),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateRaw(this, &FSubSection::TogglePlayableDirectly),
					FCanExecuteAction::CreateLambda([]{ return true; }),
					FGetActionCheckState::CreateRaw(this, &FSubSection::IsPlayableDirectly)
				),
				NAME_None,
				EUserInterfaceActionType::ToggleButton
			);
		}
		MenuBuilder.EndSection();
	}

	void TogglePlayableDirectly()
	{
		TSharedPtr<ISequencer> Sequencer = GetSequencer();
		if (Sequencer)
		{
			FScopedTransaction Transaction(LOCTEXT("SetPlayableDirectly_Transaction", "Set Playable Directly"));

			TArray<UMovieSceneSection*> SelectedSections;
			Sequencer->GetSelectedSections(SelectedSections);

			const bool bNewPlayableDirectly = IsPlayableDirectly() != ECheckBoxState::Checked;

			for (UMovieSceneSection* Section : SelectedSections)
			{
				if (UMovieSceneSubSection* SubSection = Cast<UMovieSceneSubSection>(Section))
				{
					UMovieSceneSequence* Sequence = SubSection->GetSequence();
					if (Sequence->IsPlayableDirectly() != bNewPlayableDirectly)
					{
						Sequence->SetPlayableDirectly(bNewPlayableDirectly);
					}
				}
			}
		}
	}

	ECheckBoxState IsPlayableDirectly() const
	{
		ECheckBoxState CheckboxState = ECheckBoxState::Undetermined;

		TSharedPtr<ISequencer> Sequencer = GetSequencer();
		if (Sequencer)
		{
			TArray<UMovieSceneSection*> SelectedSections;
			Sequencer->GetSelectedSections(SelectedSections);

			for (UMovieSceneSection* Section : SelectedSections)
			{
				if (UMovieSceneSubSection* SubSection = Cast<UMovieSceneSubSection>(Section))
				{
					UMovieSceneSequence* Sequence = SubSection->GetSequence();
					if (Sequence)
					{
						if (CheckboxState == ECheckBoxState::Undetermined)
						{
							CheckboxState = Sequence->IsPlayableDirectly() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
						}
						else if (CheckboxState == ECheckBoxState::Checked != Sequence->IsPlayableDirectly())
						{
							return ECheckBoxState::Undetermined;
						}
					}
				}
			}
		}

		return CheckboxState;
	}

	virtual bool IsReadOnly() const override
	{
		// Overridden to false regardless of movie scene section read only state so that we can double click into the sub section
		return false;
	}

private:

	/** The sub track editor that contains this section */
	TWeakPtr<FSubTrackEditor> SubTrackEditor;
};


/* FSubTrackEditor structors
 *****************************************************************************/

FSubTrackEditor::FSubTrackEditor(TSharedRef<ISequencer> InSequencer)
	: FMovieSceneTrackEditor(InSequencer) 
{ }


/* ISequencerTrackEditor interface
 *****************************************************************************/

void FSubTrackEditor::BuildAddTrackMenu(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.AddMenuEntry(
		FText::Join(FText::FromString(" "), GetSubTrackName(), LOCTEXT("TrackText", "Track")),
		GetSubTrackToolTip(),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), GetSubTrackBrushName()),
		FUIAction(
			FExecuteAction::CreateRaw(this, &FSubTrackEditor::HandleAddSubTrackMenuEntryExecute),
			FCanExecuteAction::CreateRaw(this, &FSubTrackEditor::HandleAddSubTrackMenuEntryCanExecute)
		)
	);
}

TSharedPtr<SWidget> FSubTrackEditor::BuildOutlinerEditWidget(const FGuid& ObjectBinding, UMovieSceneTrack* Track, const FBuildEditWidgetParams& Params)
{
	return UE::Sequencer::MakeAddButton(GetSubTrackName(), FOnGetContent::CreateSP(this, &FSubTrackEditor::HandleAddSubSequenceComboButtonGetMenuContent, Track), Params.ViewModel);
}


TSharedRef<ISequencerTrackEditor> FSubTrackEditor::CreateTrackEditor(TSharedRef<ISequencer> InSequencer)
{
	return MakeShareable(new FSubTrackEditor(InSequencer));
}


TSharedRef<ISequencerSection> FSubTrackEditor::MakeSectionInterface(UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding)
{
	return MakeShareable(new FSubSection(GetSequencer(), SectionObject, SharedThis(this)));
}


bool FSubTrackEditor::CanHandleAssetAdded(UMovieSceneSequence* Sequence) const
{
	// Only allow sequences without a camera cut track to be dropped as a subsequence. Otherwise, it'll be dropped as a shot.
	return Sequence->GetMovieScene()->GetCameraCutTrack() == nullptr;
}

bool FSubTrackEditor::HandleAssetAdded(UObject* Asset, const FGuid& TargetObjectGuid)
{
	UMovieSceneSequence* Sequence = Cast<UMovieSceneSequence>(Asset);

	if (Sequence == nullptr)
	{
		return false;
	}

	if (!SupportsSequence(Sequence))
	{
		return false;
	}

	if (!CanHandleAssetAdded(Sequence))
	{
		return false;
	}

	if (Sequence->GetMovieScene()->GetPlaybackRange().IsEmpty())
	{
		FNotificationInfo Info(FText::Format(LOCTEXT("InvalidSequenceDuration", "Invalid level sequence {0}. The sequence has no duration."), Sequence->GetDisplayName()));
		Info.bUseLargeFont = false;
		FSlateNotificationManager::Get().AddNotification(Info);
		return false;
	}

	if (CanAddSubSequence(*Sequence))
	{
		const FScopedTransaction Transaction(FText::Join(FText::FromString(" "), LOCTEXT("AddText", "Add"), GetSubTrackName(), LOCTEXT("TrackText", "Track")));

		int32 RowIndex = INDEX_NONE;
		UMovieSceneTrack* Track = nullptr;
		AnimatablePropertyChanged(FOnKeyProperty::CreateRaw(this, &FSubTrackEditor::HandleSequenceAdded, Sequence, Track, RowIndex));

		return true;
	}

	FNotificationInfo Info(FText::Format( LOCTEXT("InvalidSequence", "Invalid level sequence {0}. There could be a circular dependency."), Sequence->GetDisplayName()));	
	Info.bUseLargeFont = false;
	FSlateNotificationManager::Get().AddNotification(Info);

	return false;
}

bool FSubTrackEditor::SupportsSequence(UMovieSceneSequence* InSequence) const
{
	ETrackSupport TrackSupported = InSequence ? InSequence->IsTrackSupported(UMovieSceneSubTrack::StaticClass()) : ETrackSupport::NotSupported;
	return TrackSupported == ETrackSupport::Supported;
}

bool FSubTrackEditor::SupportsType(TSubclassOf<UMovieSceneTrack> Type) const
{
	return Type == GetSubTrackClass();
}

const FSlateBrush* FSubTrackEditor::GetIconBrush() const
{
	return FAppStyle::GetBrush(GetSubTrackBrushName());
}

bool FSubTrackEditor::OnAllowDrop(const FDragDropEvent& DragDropEvent, FSequencerDragDropParams& DragDropParams)
{
	if (!DragDropParams.Track.IsValid())
	{
		return false;
	}

	if (!DragDropParams.Track.Get()->IsA(GetSubTrackClass()))
	{
		return false;
	}

	TSharedPtr<FDragDropOperation> Operation = DragDropEvent.GetOperation();

	if (!Operation.IsValid() || !Operation->IsOfType<FAssetDragDropOp>() )
	{
		return false;
	}
	
	TSharedPtr<ISequencer> SequencerPtr = GetSequencer();
	if (!SequencerPtr)
	{
		return false;
	}

	UMovieSceneSequence* FocusedSequence = SequencerPtr->GetFocusedMovieSceneSequence();
	if (!FocusedSequence)
	{
		return false;
	}

	TSharedPtr<FAssetDragDropOp> DragDropOp = StaticCastSharedPtr<FAssetDragDropOp>( Operation );

	TOptional<FFrameNumber> LongestLengthInFrames;
	for (const FAssetData& AssetData : DragDropOp->GetAssets())
	{
		if (!MovieSceneToolHelpers::IsValidAsset(FocusedSequence, AssetData))
		{
			continue;
		}

		UMovieSceneSequence* Sequence = Cast<UMovieSceneSequence>(AssetData.GetAsset());
		if (Sequence && CanAddSubSequence(*Sequence))
		{
			FFrameRate TickResolution = SequencerPtr->GetFocusedTickResolution();

			const FQualifiedFrameTime InnerDuration = FQualifiedFrameTime(
				UE::MovieScene::DiscreteSize(Sequence->GetMovieScene()->GetPlaybackRange()),
				Sequence->GetMovieScene()->GetTickResolution());

			FFrameNumber LengthInFrames = InnerDuration.ConvertTo(TickResolution).FrameNumber;
			
			// Keep track of the longest sub-sequence asset we're trying to drop onto it for preview display purposes.
			LongestLengthInFrames = FMath::Max(LongestLengthInFrames.Get(FFrameNumber(0)), LengthInFrames);
		}
	}

	if (LongestLengthInFrames.IsSet())
	{
		DragDropParams.FrameRange = TRange<FFrameNumber>(DragDropParams.FrameNumber, DragDropParams.FrameNumber + LongestLengthInFrames.GetValue());
		return true;
	}

	return false;
}

FReply FSubTrackEditor::OnDrop(const FDragDropEvent& DragDropEvent, const FSequencerDragDropParams& DragDropParams)
{
	if (!DragDropParams.Track.IsValid())
	{
		return FReply::Unhandled();
	}

	if (!DragDropParams.Track.Get()->IsA(GetSubTrackClass()))
	{
		return FReply::Unhandled();
	}

	TSharedPtr<FDragDropOperation> Operation = DragDropEvent.GetOperation();

	if (!Operation.IsValid() || !Operation->IsOfType<FAssetDragDropOp>() )
	{
		return FReply::Unhandled();
	}
	
	TSharedPtr<ISequencer> SequencerPtr = GetSequencer();
	if (!SequencerPtr)
	{
		return FReply::Unhandled();
	}

	UMovieSceneSequence* FocusedSequence = SequencerPtr->GetFocusedMovieSceneSequence();
	if (!FocusedSequence)
	{
		return FReply::Unhandled();
	}

	const FScopedTransaction Transaction(LOCTEXT("DropAssets", "Drop Assets"));

	TSharedPtr<FAssetDragDropOp> DragDropOp = StaticCastSharedPtr<FAssetDragDropOp>( Operation );
	
	FMovieSceneTrackEditor::BeginKeying(DragDropParams.FrameNumber);

	bool bAnyDropped = false;
	for (const FAssetData& AssetData : DragDropOp->GetAssets())
	{
		if (!MovieSceneToolHelpers::IsValidAsset(FocusedSequence, AssetData))
		{
			continue;
		}

		UMovieSceneSequence* Sequence = Cast<UMovieSceneSequence>(AssetData.GetAsset());
		if (CanAddSubSequence(*Sequence))
		{
			AnimatablePropertyChanged(FOnKeyProperty::CreateRaw(this, &FSubTrackEditor::HandleSequenceAdded, Sequence, DragDropParams.Track.Get(), DragDropParams.RowIndex));

			bAnyDropped = true;
		}
	}

	FMovieSceneTrackEditor::EndKeying();

	return bAnyDropped ? FReply::Handled() : FReply::Unhandled();
}

bool FSubTrackEditor::IsResizable(UMovieSceneTrack* InTrack) const
{
	return true;
}

void FSubTrackEditor::Resize(float NewSize, UMovieSceneTrack* InTrack)
{
	UMovieSceneSubTrack* SubTrack = Cast<UMovieSceneSubTrack>(InTrack);
	if (SubTrack)
	{
		SubTrack->Modify();

		const int32 MaxNumRows = SubTrack->GetMaxRowIndex() + 1;
		SubTrack->SetRowHeight(FMath::RoundToInt(NewSize) / MaxNumRows);
		SubTrack->SetRowHeight(NewSize);
	}
}

/* FSubTrackEditor
 *****************************************************************************/

void FSubTrackEditor::InsertSection(UMovieSceneTrack* Track)
{
	FFrameTime NewSectionStartTime = GetSequencer()->GetLocalTime().Time;

	UMovieScene* MovieScene = GetFocusedMovieScene();
	if (!MovieScene)
	{
		return;
	}

	UMovieSceneSubTrack* SubTrack = FindOrCreateSubTrack(MovieScene, Track);

	FString NewSequenceName = MovieSceneToolHelpers::GenerateNewSubsequenceName(SubTrack->GetAllSections(), GetDefaultSubsequenceName(), NewSectionStartTime.FrameNumber);
	FString NewSequencePath = MovieSceneToolHelpers::GenerateNewSubsequencePath(GetSequencer()->GetFocusedMovieSceneSequence()->GetMovieScene(), GetDefaultSubsequenceDirectory(), NewSequenceName);

	FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");
	AssetToolsModule.Get().CreateUniqueAssetName(NewSequencePath + TEXT("/") + NewSequenceName, TEXT(""), NewSequencePath, NewSequenceName);

	if (UMovieSceneSequence* NewSequence = MovieSceneToolHelpers::CreateSequence(NewSequenceName, NewSequencePath))
	{
		const FScopedTransaction Transaction(FText::Join(FText::FromString(" "), LOCTEXT("InsertText", "Insert"), GetSubTrackName()));

		int32 Duration = UE::MovieScene::DiscreteSize(NewSequence->GetMovieScene()->GetPlaybackRange());

		if (UMovieSceneSubSection* NewSection = SubTrack->AddSequence(NewSequence, NewSectionStartTime.FrameNumber, Duration))
		{
			NewSection->SetRowIndex(MovieSceneToolHelpers::FindAvailableRowIndex(Track, NewSection));

			GetSequencer()->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemAdded);
			GetSequencer()->EmptySelection();
			GetSequencer()->SelectSection(NewSection);
			GetSequencer()->ThrobSectionSelection();
		}
	}
}

void FSubTrackEditor::DuplicateSection(UMovieSceneSubSection* Section)
{
	UMovieSceneSubTrack* SubTrack = CastChecked<UMovieSceneSubTrack>(Section->GetOuter());

	FFrameNumber StartTime = Section->HasStartFrame() ? Section->GetInclusiveStartFrame() : 0;
	FString NewSectionName = MovieSceneToolHelpers::GenerateNewSubsequenceName(SubTrack->GetAllSections(), GetDefaultSubsequenceName(), StartTime);
	FString NewSequencePath = FPaths::GetPath(Section->GetSequence()->GetPathName());

	// Duplicate the section and put it on the next available row
	UMovieSceneSequence* NewSequence = MovieSceneToolHelpers::CreateSequence(NewSectionName, NewSequencePath, Section);
	if (NewSequence)
	{
		const FScopedTransaction Transaction(FText::Join(FText::FromString(" "), LOCTEXT("DuplicateText", "Duplicate"), GetSubTrackName()));

		int32 Duration = UE::MovieScene::DiscreteSize(Section->GetRange());

		if (UMovieSceneSubSection* NewSection = SubTrack->AddSequence(NewSequence, StartTime, Duration))
		{
			NewSection->SetRange(Section->GetRange());
			NewSection->SetRowIndex(MovieSceneToolHelpers::FindAvailableRowIndex(SubTrack, NewSection));
			NewSection->Parameters.StartFrameOffset = Section->Parameters.StartFrameOffset;
			NewSection->Parameters.TimeScale = Section->Parameters.TimeScale;
			NewSection->SetPreRollFrames(Section->GetPreRollFrames());
			NewSection->SetColorTint(Section->GetColorTint());

			GetSequencer()->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemAdded);
			GetSequencer()->EmptySelection();
			GetSequencer()->SelectSection(NewSection);
			GetSequencer()->ThrobSectionSelection();
		}
	}
}

void FSubTrackEditor::CreateNewTake(UMovieSceneSubSection* Section)
{
	FString ShotPrefix;
	uint32 ShotNumber = INDEX_NONE;
	uint32 TakeNumber = INDEX_NONE;
	uint32 ShotNumberDigits = 0;
	uint32 TakeNumberDigits = 0;
	
	FString SequenceName = Section->GetSequence() ? Section->GetSequence()->GetName() : FString();

	if (MovieSceneToolHelpers::ParseShotName(SequenceName, ShotPrefix, ShotNumber, TakeNumber, ShotNumberDigits, TakeNumberDigits))
	{
		TArray<FAssetData> AssetData;
		uint32 CurrentTakeNumber = INDEX_NONE;
		MovieSceneToolHelpers::GatherTakes(Section, AssetData, CurrentTakeNumber);
		uint32 NewTakeNumber = CurrentTakeNumber;

		for (auto ThisAssetData : AssetData)
		{
			uint32 ThisTakeNumber = INDEX_NONE;
			if (MovieSceneToolHelpers::GetTakeNumber(Section, ThisAssetData, ThisTakeNumber))
			{
				if (ThisTakeNumber >= NewTakeNumber)
				{
					NewTakeNumber = ThisTakeNumber + 1;
				}
			}
		}

		FString NewSectionName = MovieSceneToolHelpers::ComposeShotName(ShotPrefix, ShotNumber, NewTakeNumber, ShotNumberDigits, TakeNumberDigits);

		TRange<FFrameNumber> NewSectionRange         = Section->GetRange();
		FFrameNumber         NewSectionStartOffset   = Section->Parameters.StartFrameOffset;
		float                NewSectionTimeScale     = Section->Parameters.TimeScale;
		int32                NewSectionPrerollFrames = Section->GetPreRollFrames();
		int32                NewRowIndex          = Section->GetRowIndex();
		FFrameNumber         NewSectionStartTime     = NewSectionRange.GetLowerBound().IsClosed() ? UE::MovieScene::DiscreteInclusiveLower(NewSectionRange) : 0;
		FColor               NewSectionColorTint     = Section->GetColorTint();
		UMovieSceneSubTrack* SubTrack = CastChecked<UMovieSceneSubTrack>(Section->GetOuter());
		FString NewSequencePath = FPaths::GetPath(Section->GetSequence()->GetPathName());

		if (UMovieSceneSequence* NewSequence = MovieSceneToolHelpers::CreateSequence(NewSectionName, NewSequencePath, Section))
		{
			const FScopedTransaction Transaction(LOCTEXT("NewTake_Transaction", "New Take"));

			int32 Duration = UE::MovieScene::DiscreteSize(Section->GetRange());

			UMovieSceneSubSection* NewSection = SubTrack->AddSequence(NewSequence, NewSectionStartTime, Duration);
			SubTrack->RemoveSection(*Section);

			NewSection->SetRange(NewSectionRange);
			NewSection->Parameters.StartFrameOffset = NewSectionStartOffset;
			NewSection->Parameters.TimeScale = NewSectionTimeScale;
			NewSection->SetPreRollFrames(NewSectionPrerollFrames);
			NewSection->SetRowIndex(NewRowIndex);
			NewSection->SetColorTint(NewSectionColorTint);

			UMovieSceneCinematicShotSection* ShotSection = Cast<UMovieSceneCinematicShotSection>(Section);
			UMovieSceneCinematicShotSection* NewShotSection = Cast<UMovieSceneCinematicShotSection>(NewSection);

			// If the old shot's name is not the same as the sequence's name, assume the user had customized the shot name, so carry it over
			if (ShotSection && NewShotSection && ShotSection->GetSequence() && ShotSection->GetShotDisplayName() != ShotSection->GetSequence()->GetName())
			{
				NewShotSection->SetShotDisplayName(ShotSection->GetShotDisplayName());
			}

			MovieSceneToolHelpers::SetTakeNumber(NewSection, NewTakeNumber);

			GetSequencer()->NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::MovieSceneStructureItemsChanged );
			GetSequencer()->EmptySelection();
			GetSequencer()->SelectSection(NewSection);
			GetSequencer()->ThrobSectionSelection();
		}
	}
}

void FSubTrackEditor::SwitchTake(UObject* TakeObject)
{
	ChangeTake(Cast<UMovieSceneSequence>(TakeObject));
}

void FSubTrackEditor::ChangeTake(UMovieSceneSequence* Sequence)
{
	bool bChangedTake = false;

	const FScopedTransaction Transaction(LOCTEXT("ChangeTake_Transaction", "Change Take"));

	TArray<UMovieSceneSection*> Sections;
	GetSequencer()->GetSelectedSections(Sections);

	for (int32 SectionIndex = 0; SectionIndex < Sections.Num(); ++SectionIndex)
	{
		if (!Sections[SectionIndex]->IsA<UMovieSceneSubSection>())
		{
			continue;
		}

		UMovieSceneSubSection* Section = Cast<UMovieSceneSubSection>(Sections[SectionIndex]);
		UMovieSceneSubTrack* SubTrack = CastChecked<UMovieSceneSubTrack>(Section->GetOuter());

		TRange<FFrameNumber> NewSectionRange = Section->GetRange();
		FFrameNumber		 NewSectionStartOffset = Section->Parameters.StartFrameOffset;
		float                NewSectionTimeScale = Section->Parameters.TimeScale;
		int32                NewSectionPrerollFrames = Section->GetPreRollFrames();
		int32                NewRowIndex = Section->GetRowIndex();
		FFrameNumber         NewSectionStartTime = NewSectionRange.GetLowerBound().IsClosed() ? UE::MovieScene::DiscreteInclusiveLower(NewSectionRange) : 0;
		int32                NewSectionRowIndex = Section->GetRowIndex();
		FColor               NewSectionColorTint = Section->GetColorTint();

		const int32 Duration = (NewSectionRange.GetLowerBound().IsClosed() && NewSectionRange.GetUpperBound().IsClosed()) ? UE::MovieScene::DiscreteSize(NewSectionRange) : 1;
		UMovieSceneSubSection* NewSection = SubTrack->AddSequence(Sequence, NewSectionStartTime, Duration);

		if (NewSection != nullptr)
		{
			SubTrack->RemoveSection(*Section);

			NewSection->SetRange(NewSectionRange);
			NewSection->Parameters.StartFrameOffset = NewSectionStartOffset;
			NewSection->Parameters.TimeScale = NewSectionTimeScale;
			NewSection->SetPreRollFrames(NewSectionPrerollFrames);
			NewSection->SetRowIndex(NewSectionRowIndex);
			NewSection->SetColorTint(NewSectionColorTint);

			UMovieSceneCinematicShotSection* ShotSection = Cast<UMovieSceneCinematicShotSection>(Section);
			UMovieSceneCinematicShotSection* NewShotSection = Cast<UMovieSceneCinematicShotSection>(NewSection);

			// If the old shot's name is not the same as the sequence's name, assume the user had customized the shot name, so carry it over
			if (ShotSection && NewShotSection && ShotSection->GetSequence() && ShotSection->GetShotDisplayName() != ShotSection->GetSequence()->GetName())
			{
				NewShotSection->SetShotDisplayName(ShotSection->GetShotDisplayName());
			}

			bChangedTake = true;
		}
	}

	if (bChangedTake)
	{
		GetSequencer()->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemsChanged);
	}
}

void FSubTrackEditor::AddTakesMenu(UMovieSceneSubSection* Section, FMenuBuilder& MenuBuilder)
{
	TArray<FAssetData> AssetData;
	uint32 CurrentTakeNumber = INDEX_NONE;
	MovieSceneToolHelpers::GatherTakes(Section, AssetData, CurrentTakeNumber);

	AssetData.Sort([Section](const FAssetData& A, const FAssetData& B) {
		uint32 TakeNumberA = INDEX_NONE;
		uint32 TakeNumberB = INDEX_NONE;
		if (MovieSceneToolHelpers::GetTakeNumber(Section, A, TakeNumberA) && MovieSceneToolHelpers::GetTakeNumber(Section, B, TakeNumberB))
		{
			return TakeNumberA < TakeNumberB;
		}
		return true;
	});

	for (auto ThisAssetData : AssetData)
	{
		uint32 TakeNumber = INDEX_NONE;
		if (MovieSceneToolHelpers::GetTakeNumber(Section, ThisAssetData, TakeNumber))
		{
			UMovieSceneSequence* Sequence = Cast<UMovieSceneSequence>(ThisAssetData.GetAsset());
			if (Sequence)
			{
				FText MetaDataText = FSubTrackEditorUtil::GetMetaDataText(Sequence);
				MenuBuilder.AddMenuEntry(
					FText::Format(LOCTEXT("TakeNumber", "Take {0}"), FText::AsNumber(TakeNumber)),
					MetaDataText.IsEmpty() ? 
					FText::Format(LOCTEXT("TakeNumberTooltip", "Change to {0}"), FText::FromString(Sequence->GetPathName())) : 
					FText::Format(LOCTEXT("TakeNumberWithMetaDataTooltip", "Change to {0}\n\n{1}"), FText::FromString(Sequence->GetPathName()), MetaDataText),
					TakeNumber == CurrentTakeNumber ? FSlateIcon(FAppStyle::GetAppStyleSetName(), "Sequencer.Star") : FSlateIcon(FAppStyle::GetAppStyleSetName(), "Sequencer.Empty"),
					FUIAction(FExecuteAction::CreateSP(this, &FSubTrackEditor::ChangeTake, Sequence))
				);
			}
		}
	}
}

TWeakPtr<SWindow> MetaDataWindow;

void FSubTrackEditor::EditMetaData(UMovieSceneSubSection* Section)
{
	UMovieSceneSequence* Sequence = Section->GetSequence();
	if (!Sequence)
	{
		return;
	}

	UMovieSceneMetaData* MetaData = FSubTrackEditorUtil::FindOrAddMetaData(Sequence);
	if (!MetaData)
	{
		return;
	}

	TSharedPtr<SWindow> ExistingWindow = MetaDataWindow.Pin();
	if (ExistingWindow.IsValid())
	{
		ExistingWindow->BringToFront();
	}
	else
	{
		ExistingWindow = SNew(SWindow)
			.Title(FText::Format(LOCTEXT("MetaDataTitle", "Edit {0}"), FText::FromString(GetSubSectionDisplayName(Section))))
			.HasCloseButton(true)
			.SupportsMaximize(false)
			.SupportsMinimize(false)
			.ClientSize(FVector2D(400, 200));

		TSharedPtr<SWindow> ParentWindow;
		if (FModuleManager::Get().IsModuleLoaded("MainFrame"))
		{
			IMainFrameModule& MainFrame = FModuleManager::LoadModuleChecked<IMainFrameModule>("MainFrame");
			ParentWindow = MainFrame.GetParentWindow();
		}

		if (ParentWindow.IsValid())
		{
			FSlateApplication::Get().AddWindowAsNativeChild(ExistingWindow.ToSharedRef(), ParentWindow.ToSharedRef());
		}
		else
		{
			FSlateApplication::Get().AddWindow(ExistingWindow.ToSharedRef());
		}
	}

	FPropertyEditorModule& EditModule = FModuleManager::Get().GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bAllowSearch = false;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsViewArgs.bHideSelectionTip = true;
	DetailsViewArgs.bShowOptions = false;
	DetailsViewArgs.bShowScrollBar = false;

	TSharedRef<IDetailsView> DetailsView = EditModule.CreateDetailView(DetailsViewArgs);
	TArray<UObject*> Objects;
	Objects.Add(MetaData);
	DetailsView->SetObjects(Objects, true);

	ExistingWindow->SetContent(DetailsView);

	MetaDataWindow = ExistingWindow;
}

bool FSubTrackEditor::CanAddSubSequence(const UMovieSceneSequence& Sequence) const
{
	UMovieSceneSequence* FocusedSequence = GetSequencer()->GetFocusedMovieSceneSequence();
	return FSubTrackEditorUtil::CanAddSubSequence(FocusedSequence, Sequence);
}

FText FSubTrackEditor::GetSubTrackName() const
{
	return LOCTEXT("SubTrackName", "Subsequence");
}

FText FSubTrackEditor::GetSubTrackToolTip() const
{ 
	return LOCTEXT("SubTrackToolTip", "A track that can contain other sequences.");
}

FName FSubTrackEditor::GetSubTrackBrushName() const
{
	return TEXT("Sequencer.Tracks.Sub");
}

FString FSubTrackEditor::GetSubSectionDisplayName(const UMovieSceneSubSection* Section) const
{
	return Section && Section->GetSequence() ? Section->GetSequence()->GetName() : FString();
}

FString FSubTrackEditor::GetDefaultSubsequenceName() const
{
	const UMovieSceneToolsProjectSettings* ProjectSettings = GetDefault<UMovieSceneToolsProjectSettings>();
	return ProjectSettings->SubsequencePrefix;
}

FString FSubTrackEditor::GetDefaultSubsequenceDirectory() const
{
	const UMovieSceneToolsProjectSettings* ProjectSettings = GetDefault<UMovieSceneToolsProjectSettings>();
	return ProjectSettings->SubsequenceDirectory;
}

TSubclassOf<UMovieSceneSubTrack> FSubTrackEditor::GetSubTrackClass() const
{
	return UMovieSceneSubTrack::StaticClass();
}

void FSubTrackEditor::GetSupportedSequenceClassPaths(TArray<FTopLevelAssetPath>& ClassPaths) const
{
	ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/LevelSequence"), TEXT("LevelSequence")));
}

UMovieSceneSubTrack* FSubTrackEditor::CreateNewTrack(UMovieScene* MovieScene) const
{
	return Cast<UMovieSceneSubTrack>(MovieScene->AddTrack(GetSubTrackClass()));
}

/* FSubTrackEditor callbacks
 *****************************************************************************/

void FSubTrackEditor::HandleAddSubTrackMenuEntryExecute()
{
	UMovieScene* FocusedMovieScene = GetFocusedMovieScene();

	if (FocusedMovieScene == nullptr)
	{
		return;
	}

	if (FocusedMovieScene->IsReadOnly())
	{
		return;
	}

	const FScopedTransaction Transaction(FText::Join(FText::FromString(" "), LOCTEXT("AddText", "Add"), GetSubTrackName(), LOCTEXT("TrackText", "Track")));
	FocusedMovieScene->Modify();

	UMovieSceneSubTrack* NewTrack = FindOrCreateSubTrack(FocusedMovieScene, nullptr);
	ensure(NewTrack);

	if (GetSequencer().IsValid())
	{
		GetSequencer()->OnAddTrack(NewTrack, FGuid());
	}
}

UMovieSceneSubTrack* FSubTrackEditor::FindOrCreateSubTrack(UMovieScene* MovieScene, UMovieSceneTrack* Track) const
{
	UMovieSceneSubTrack* SubTrack = Cast<UMovieSceneSubTrack>(Track);
	if (!SubTrack)
	{
		SubTrack = Cast<UMovieSceneSubTrack>(MovieScene->AddTrack(GetSubTrackClass()));
	}
	return SubTrack;
}

TSharedRef<SWidget> FSubTrackEditor::HandleAddSubSequenceComboButtonGetMenuContent(UMovieSceneTrack* InTrack)
{
	FMenuBuilder MenuBuilder(true, nullptr);

	MenuBuilder.AddMenuEntry(
		FText::Join(FText::FromString(" "), LOCTEXT("InsertText", "Insert"), GetSubTrackName()),
		LOCTEXT("InsertSectionTooltip", "Insert new sequence at current time"),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateSP(this, &FSubTrackEditor::InsertSection, InTrack))
	);

	MenuBuilder.BeginSection(TEXT("ChooseSequence"), LOCTEXT("ChooseSequence", "Choose Sequence"));
	{
		UMovieSceneSequence* Sequence = GetSequencer() ? GetSequencer()->GetFocusedMovieSceneSequence() : nullptr;

		FAssetPickerConfig AssetPickerConfig;
		{
			AssetPickerConfig.OnAssetSelected = FOnAssetSelected::CreateRaw( this, &FSubTrackEditor::HandleAddSubSequenceComboButtonMenuEntryExecute, InTrack);
			AssetPickerConfig.OnAssetEnterPressed = FOnAssetEnterPressed::CreateRaw( this, &FSubTrackEditor::HandleAddSubSequenceComboButtonMenuEntryEnterPressed, InTrack);
			AssetPickerConfig.bAllowNullSelection = false;
			AssetPickerConfig.bAddFilterUI = true;
			AssetPickerConfig.InitialAssetViewType = EAssetViewType::List;
			GetSupportedSequenceClassPaths(AssetPickerConfig.Filter.ClassPaths);
			AssetPickerConfig.SaveSettingsName = TEXT("SequencerAssetPicker");
			AssetPickerConfig.AdditionalReferencingAssets.Add(FAssetData(Sequence));
		}

		FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

		TSharedPtr<SBox> MenuEntry = SNew(SBox)
			.WidthOverride(300.0f)
			.HeightOverride(300.f)
			[
				ContentBrowserModule.Get().CreateAssetPicker(AssetPickerConfig)
			];

		MenuBuilder.AddWidget(MenuEntry.ToSharedRef(), FText::GetEmpty(), true);
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

void FSubTrackEditor::HandleAddSubSequenceComboButtonMenuEntryExecute(const FAssetData& AssetData, UMovieSceneTrack* InTrack)
{
	FSlateApplication::Get().DismissAllMenus();

	UObject* SelectedObject = AssetData.GetAsset();

	if (SelectedObject && SelectedObject->IsA(UMovieSceneSequence::StaticClass()))
	{
		UMovieSceneSequence* MovieSceneSequence = CastChecked<UMovieSceneSequence>(AssetData.GetAsset());

		int32 RowIndex = INDEX_NONE;
		AnimatablePropertyChanged( FOnKeyProperty::CreateRaw( this, &FSubTrackEditor::AddKeyInternal, MovieSceneSequence, InTrack, RowIndex) );
	}
}

void FSubTrackEditor::HandleAddSubSequenceComboButtonMenuEntryEnterPressed(const TArray<FAssetData>& AssetData, UMovieSceneTrack* InTrack)
{
	if (AssetData.Num() > 0)
	{
		HandleAddSubSequenceComboButtonMenuEntryExecute(AssetData[0].GetAsset(), InTrack);
	}
}

FKeyPropertyResult FSubTrackEditor::AddKeyInternal(FFrameNumber KeyTime, UMovieSceneSequence* InMovieSceneSequence, UMovieSceneTrack* InTrack, int32 RowIndex)
{	
	FKeyPropertyResult KeyPropertyResult;

	if (InMovieSceneSequence->GetMovieScene()->GetPlaybackRange().IsEmpty())
	{
		FNotificationInfo Info(FText::Format(LOCTEXT("InvalidSequenceDuration", "Invalid level sequence {0}. The sequence has no duration."), InMovieSceneSequence->GetDisplayName()));
		Info.bUseLargeFont = false;
		FSlateNotificationManager::Get().AddNotification(Info);
		return KeyPropertyResult;
	}

	if (CanAddSubSequence(*InMovieSceneSequence))
	{
		UMovieScene* MovieScene = GetFocusedMovieScene();

		UMovieSceneSubTrack* SubTrack = FindOrCreateSubTrack(MovieScene, InTrack);

		const FFrameRate TickResolution = InMovieSceneSequence->GetMovieScene()->GetTickResolution();
		const FQualifiedFrameTime InnerDuration = FQualifiedFrameTime(
			UE::MovieScene::DiscreteSize(InMovieSceneSequence->GetMovieScene()->GetPlaybackRange()),
			TickResolution);

		const FFrameRate OuterFrameRate = SubTrack->GetTypedOuter<UMovieScene>()->GetTickResolution();
		const int32      OuterDuration  = InnerDuration.ConvertTo(OuterFrameRate).FrameNumber.Value;

		UMovieSceneSubSection* NewSection = SubTrack->AddSequenceOnRow(InMovieSceneSequence, KeyTime, OuterDuration, RowIndex);
		KeyPropertyResult.bTrackModified = true;
		KeyPropertyResult.SectionsCreated.Add(NewSection);

		GetSequencer()->EmptySelection();
		GetSequencer()->SelectSection(NewSection);
		GetSequencer()->ThrobSectionSelection();

		if (TickResolution != OuterFrameRate)
		{
			FNotificationInfo Info(FText::Format(LOCTEXT("TickResolutionMismatch", "The parent sequence has a different tick resolution {0} than the newly added sequence {1}"), OuterFrameRate.ToPrettyText(), TickResolution.ToPrettyText()));
			Info.bUseLargeFont = false;
			FSlateNotificationManager::Get().AddNotification(Info);
		}

		return KeyPropertyResult;
	}

	FNotificationInfo Info(FText::Format( LOCTEXT("InvalidSequence", "Invalid level sequence {0}. There could be a circular dependency."), InMovieSceneSequence->GetDisplayName()));
	Info.bUseLargeFont = false;
	FSlateNotificationManager::Get().AddNotification(Info);

	return KeyPropertyResult;
}

FKeyPropertyResult FSubTrackEditor::HandleSequenceAdded(FFrameNumber KeyTime, UMovieSceneSequence* Sequence, UMovieSceneTrack* Track, int32 RowIndex)
{
	FKeyPropertyResult KeyPropertyResult;

	UMovieScene* MovieScene = GetFocusedMovieScene();

	UMovieSceneSubTrack* SubTrack = FindOrCreateSubTrack(MovieScene, Track);

	const FFrameRate TickResolution = Sequence->GetMovieScene()->GetTickResolution();
	const FQualifiedFrameTime InnerDuration = FQualifiedFrameTime(
		UE::MovieScene::DiscreteSize(Sequence->GetMovieScene()->GetPlaybackRange()),
		TickResolution);

	const FFrameRate OuterFrameRate = SubTrack->GetTypedOuter<UMovieScene>()->GetTickResolution();
	const int32      OuterDuration  = InnerDuration.ConvertTo(OuterFrameRate).FrameNumber.Value;

	UMovieSceneSubSection* NewSection = SubTrack->AddSequenceOnRow(Sequence, KeyTime, OuterDuration, RowIndex);
	KeyPropertyResult.bTrackModified = true;
	KeyPropertyResult.SectionsCreated.Add(NewSection);

	GetSequencer()->EmptySelection();
	GetSequencer()->SelectSection(NewSection);
	GetSequencer()->ThrobSectionSelection();

	if (TickResolution != OuterFrameRate)
	{
		FNotificationInfo Info(FText::Format(LOCTEXT("TickResolutionMismatch", "The parent sequence has a different tick resolution {0} than the newly added sequence {1}"), OuterFrameRate.ToPrettyText(), TickResolution.ToPrettyText()));
		Info.bUseLargeFont = false;
		FSlateNotificationManager::Get().AddNotification(Info);
	}

	return KeyPropertyResult;
}

#undef LOCTEXT_NAMESPACE
