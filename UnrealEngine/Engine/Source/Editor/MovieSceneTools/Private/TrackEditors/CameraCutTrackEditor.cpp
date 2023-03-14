// Copyright Epic Games, Inc. All Rights Reserved.

#include "TrackEditors/CameraCutTrackEditor.h"
#include "Widgets/SBoxPanel.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Tracks/MovieSceneCameraCutTrack.h"
#include "Modules/ModuleManager.h"
#include "Application/ThrottleManager.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "MovieSceneCommonHelpers.h"
#include "Styling/AppStyle.h"
#include "GameFramework/WorldSettings.h"
#include "LevelEditorViewport.h"
#include "Sections/CameraCutSection.h"
#include "Sections/MovieSceneCameraCutSection.h"
#include "SequencerUtilities.h"
#include "Editor.h"
#include "ActorEditorUtils.h"
#include "SceneOutlinerPublicTypes.h"
#include "SceneOutlinerModule.h"
#include "ActorTreeItem.h"
#include "TrackEditorThumbnail/TrackEditorThumbnailPool.h"
#include "MovieSceneObjectBindingIDPicker.h"
#include "MovieSceneToolHelpers.h"
#include "DragAndDrop/ActorDragDropGraphEdOp.h"

#define LOCTEXT_NAMESPACE "FCameraCutTrackEditor"


class FCameraCutTrackCommands
	: public TCommands<FCameraCutTrackCommands>
{
public:

	FCameraCutTrackCommands()
		: TCommands<FCameraCutTrackCommands>
	(
		"CameraCutTrack",
		NSLOCTEXT("Contexts", "CameraCutTrack", "CameraCutTrack"),
		NAME_None, // "MainFrame" // @todo Fix this crash
		FAppStyle::GetAppStyleSetName() // Icon Style Set
	)
		, BindingCount(0)
	{ }
		
	/** Toggle the camera lock */
	TSharedPtr< FUICommandInfo > ToggleLockCamera;

	/**
	 * Initialize commands
	 */
	virtual void RegisterCommands() override;

	mutable uint32 BindingCount;
};


void FCameraCutTrackCommands::RegisterCommands()
{
	UI_COMMAND( ToggleLockCamera, "Toggle Lock Camera", "Toggle locking the viewport to the camera cut track.", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Shift, EKeys::C) );
}


/* FCameraCutTrackEditor structors
 *****************************************************************************/

FCameraCutTrackEditor::FCameraCutTrackEditor(TSharedRef<ISequencer> InSequencer)
	: FMovieSceneTrackEditor(InSequencer) 
{
	ThumbnailPool = MakeShareable(new FTrackEditorThumbnailPool(InSequencer));

	FCameraCutTrackCommands::Register();
}

void FCameraCutTrackEditor::OnRelease()
{	
	const FCameraCutTrackCommands& Commands = FCameraCutTrackCommands::Get();
	Commands.BindingCount--;
	
	if (Commands.BindingCount < 1)
	{
		FCameraCutTrackCommands::Unregister();
	}
}

TSharedRef<ISequencerTrackEditor> FCameraCutTrackEditor::CreateTrackEditor(TSharedRef<ISequencer> InSequencer)
{
	return MakeShareable(new FCameraCutTrackEditor(InSequencer));
}


/* ISequencerTrackEditor interface
 *****************************************************************************/

void FCameraCutTrackEditor::BindCommands(TSharedRef<FUICommandList> SequencerCommandBindings)
{
	const FCameraCutTrackCommands& Commands = FCameraCutTrackCommands::Get();

	SequencerCommandBindings->MapAction(
		Commands.ToggleLockCamera,
		FExecuteAction::CreateSP( this, &FCameraCutTrackEditor::ToggleLockCamera) );

	Commands.BindingCount++;
}

void FCameraCutTrackEditor::BuildAddTrackMenu(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.AddMenuEntry(
		LOCTEXT("AddCameraCutTrack", "Camera Cut Track"),
		LOCTEXT("AddCameraCutTooltip", "Adds a camera cut track, as well as a new camera cut at the current scrubber location if a camera is selected."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Sequencer.Tracks.CameraCut"),
		FUIAction(
			FExecuteAction::CreateRaw(this, &FCameraCutTrackEditor::HandleAddCameraCutTrackMenuEntryExecute),
			FCanExecuteAction::CreateRaw(this, &FCameraCutTrackEditor::HandleAddCameraCutTrackMenuEntryCanExecute)
		)
	);
}

void FCameraCutTrackEditor::BuildTrackContextMenu(FMenuBuilder& MenuBuilder, UMovieSceneTrack* Track)
{
	UMovieSceneCameraCutTrack* CameraCutTrack = Cast<UMovieSceneCameraCutTrack>(Track);
	
	MenuBuilder.AddMenuEntry(
		LOCTEXT("CanBlendShots", "Can Blend"),
		LOCTEXT("CanBlendShotsTooltip", "Enable shot blending on this track, making it possible to overlap sections."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP(this, &FCameraCutTrackEditor::HandleToggleCanBlendExecute, CameraCutTrack),
			FCanExecuteAction::CreateLambda([=]() { return CameraCutTrack != nullptr; }),
			FIsActionChecked::CreateLambda([=]() { return CameraCutTrack->bCanBlend; })
			),
		"Edit",
		EUserInterfaceActionType::ToggleButton
	);
}

void FCameraCutTrackEditor::HandleToggleCanBlendExecute(UMovieSceneCameraCutTrack* CameraCutTrack)
{
	CameraCutTrack->bCanBlend = !CameraCutTrack->bCanBlend;

	if (!CameraCutTrack->bCanBlend)
	{
		// Reset all easing and remove overlaps.
		const UMovieScene* FocusedMovieScene = GetFocusedMovieScene();
		const FFrameRate TickResolution = FocusedMovieScene->GetTickResolution();
		const FFrameRate DisplayRate = FocusedMovieScene->GetDisplayRate();

		const TArray<UMovieSceneSection*> Sections = CameraCutTrack->GetAllSections();
		for (int32 Idx = 1; Idx < Sections.Num(); ++Idx)
		{
			UMovieSceneSection* CurSection = Sections[Idx];
			UMovieSceneSection* PrevSection = Sections[Idx - 1];

			CurSection->Modify();

			TRange<FFrameNumber> CurSectionRange = CurSection->GetRange();
			TRange<FFrameNumber> PrevSectionRange = PrevSection->GetRange();
			const FFrameNumber OverlapOrGap = (PrevSectionRange.GetUpperBoundValue() - CurSectionRange.GetLowerBoundValue());
			if (OverlapOrGap > 0)
			{
				const FFrameTime TimeAtHalfBlend = CurSectionRange.GetLowerBoundValue() + FMath::FloorToInt(OverlapOrGap.Value / 2.f);
				const FFrameNumber FrameAtHalfBlend = FFrameRate::Snap(TimeAtHalfBlend, TickResolution, DisplayRate).CeilToFrame();

				PrevSectionRange.SetUpperBoundValue(FrameAtHalfBlend);
				PrevSection->SetRange(PrevSectionRange);

				CurSectionRange.SetLowerBoundValue(FrameAtHalfBlend);
				CurSection->SetRange(CurSectionRange);
			}

			CurSection->Easing.AutoEaseInDuration = 0;
			PrevSection->Easing.AutoEaseOutDuration = 0;
		}
		if (Sections.Num() > 0)
		{
			Sections[0]->Modify();

			Sections[0]->Easing.AutoEaseInDuration = 0;
			Sections[0]->Easing.ManualEaseInDuration = 0;
			Sections.Last()->Easing.AutoEaseOutDuration = 0;
			Sections.Last()->Easing.ManualEaseOutDuration = 0;
		}
	}
}

TSharedPtr<SWidget> FCameraCutTrackEditor::BuildOutlinerEditWidget(const FGuid& ObjectBinding, UMovieSceneTrack* Track, const FBuildEditWidgetParams& Params)
{
	const FCameraCutTrackCommands& Commands = FCameraCutTrackCommands::Get();
	// Create a container edit box
	return SNew(SHorizontalBox)

	// Add the camera combo box
	+ SHorizontalBox::Slot()
	.AutoWidth()
	.VAlign(VAlign_Center)
	[
		FSequencerUtilities::MakeAddButton(LOCTEXT("CameraCutText", "Camera"), FOnGetContent::CreateSP(this, &FCameraCutTrackEditor::HandleAddCameraCutComboButtonGetMenuContent), Params.NodeIsHovered, GetSequencer())
	]

	+ SHorizontalBox::Slot()
	.VAlign(VAlign_Center)
	.HAlign(HAlign_Right)
	.AutoWidth()
	.Padding(4, 0, 0, 0)
	[
		SNew(SCheckBox)
        .Style( &FAppStyle::Get().GetWidgetStyle<FCheckBoxStyle>("ToggleButtonCheckBoxAlt"))
		.Type(ESlateCheckBoxType::CheckBox)
		.Padding(FMargin(0.f))
		.IsFocusable(false)
		.IsChecked(this, &FCameraCutTrackEditor::IsCameraLocked)
		.OnCheckStateChanged(this, &FCameraCutTrackEditor::OnLockCameraClicked)
		.ToolTipText(this, &FCameraCutTrackEditor::GetLockCameraToolTip)
		.CheckedImage(FAppStyle::GetBrush("Sequencer.LockCamera"))
		.CheckedHoveredImage(FAppStyle::GetBrush("Sequencer.LockCamera"))
		.CheckedPressedImage(FAppStyle::GetBrush("Sequencer.LockCamera"))
		.UncheckedImage(FAppStyle::GetBrush("Sequencer.UnlockCamera"))
		.UncheckedHoveredImage(FAppStyle::GetBrush("Sequencer.UnlockCamera"))
		.UncheckedPressedImage(FAppStyle::GetBrush("Sequencer.UnlockCamera"))
	];
}


TSharedRef<ISequencerSection> FCameraCutTrackEditor::MakeSectionInterface(UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding)
{
	check(SupportsType(SectionObject.GetOuter()->GetClass()));

	return MakeShareable(new FCameraCutSection(GetSequencer(), ThumbnailPool, SectionObject));
}


bool FCameraCutTrackEditor::SupportsSequence(UMovieSceneSequence* InSequence) const
{
	ETrackSupport TrackSupported = InSequence ? InSequence->IsTrackSupported(UMovieSceneCameraCutTrack::StaticClass()) : ETrackSupport::NotSupported;
	return TrackSupported == ETrackSupport::Supported;
}


bool FCameraCutTrackEditor::SupportsType(TSubclassOf<UMovieSceneTrack> Type) const
{
	return (Type == UMovieSceneCameraCutTrack::StaticClass());
}


void FCameraCutTrackEditor::Tick(float DeltaTime)
{
	TSharedPtr<ISequencer> SequencerPin = GetSequencer();
	if (!SequencerPin.IsValid())
	{
		return;
	}

	EMovieScenePlayerStatus::Type PlaybackState = SequencerPin->GetPlaybackStatus();

	if (FSlateThrottleManager::Get().IsAllowingExpensiveTasks() && PlaybackState != EMovieScenePlayerStatus::Playing && PlaybackState != EMovieScenePlayerStatus::Scrubbing)
	{
		SequencerPin->EnterSilentMode();

		FQualifiedFrameTime SavedTime = SequencerPin->GetLocalTime();

		if (DeltaTime > 0.f && ThumbnailPool->DrawThumbnails())
		{
			SequencerPin->SetLocalTimeDirectly(SavedTime.Time);
		}

		SequencerPin->ExitSilentMode();
	}
}


const FSlateBrush* FCameraCutTrackEditor::GetIconBrush() const
{
	return FAppStyle::GetBrush("Sequencer.Tracks.CameraCut");
}


bool FCameraCutTrackEditor::OnAllowDrop(const FDragDropEvent& DragDropEvent, FSequencerDragDropParams& DragDropParams)
{
	if (!DragDropParams.Track.IsValid() || !DragDropParams.Track.Get()->IsA(UMovieSceneCameraCutTrack::StaticClass()))
	{
		return false;
	}

	TSharedPtr<FDragDropOperation> Operation = DragDropEvent.GetOperation();

	if (!Operation.IsValid() || !Operation->IsOfType<FActorDragDropGraphEdOp>() )
	{
		return false;
	}
	
	UMovieSceneCameraCutTrack* CameraCutTrack = Cast<UMovieSceneCameraCutTrack>(DragDropParams.Track.Get());

	TSharedPtr<FActorDragDropGraphEdOp> DragDropOp = StaticCastSharedPtr<FActorDragDropGraphEdOp>( Operation );

	for (auto& ActorPtr : DragDropOp->Actors)
	{
		if (ActorPtr.IsValid())
		{
			AActor* Actor = ActorPtr.Get();
				
			UCameraComponent* CameraComponent = MovieSceneHelpers::CameraComponentFromActor(Actor);
			if (CameraComponent)
			{
				FFrameNumber EndFrameNumber = CameraCutTrack->FindEndTimeForCameraCut(DragDropParams.FrameNumber);
				DragDropParams.FrameRange = TRange<FFrameNumber>(DragDropParams.FrameNumber, EndFrameNumber);
				return true;
			}
		}
	}

	return false;
}


FReply FCameraCutTrackEditor::OnDrop(const FDragDropEvent& DragDropEvent, const FSequencerDragDropParams& DragDropParams)
{
	if (!DragDropParams.Track.IsValid() || !DragDropParams.Track.Get()->IsA(UMovieSceneCameraCutTrack::StaticClass()))
	{
		return FReply::Unhandled();
	}

	TSharedPtr<FDragDropOperation> Operation = DragDropEvent.GetOperation();

	if (!Operation.IsValid() || !Operation->IsOfType<FActorDragDropGraphEdOp>() )
	{
		return FReply::Unhandled();
	}
	
	TSharedPtr<FActorDragDropGraphEdOp> DragDropOp = StaticCastSharedPtr<FActorDragDropGraphEdOp>( Operation );

	FMovieSceneTrackEditor::BeginKeying(DragDropParams.FrameNumber);

	bool bAnyDropped = false;
	for (auto& ActorPtr : DragDropOp->Actors)
	{
		if (ActorPtr.IsValid())
		{
			AActor* Actor = ActorPtr.Get();
				
			FGuid ObjectGuid = FindOrCreateHandleToObject(Actor).Handle;
	
			if (ObjectGuid.IsValid())
			{
				AnimatablePropertyChanged(FOnKeyProperty::CreateRaw(this, &FCameraCutTrackEditor::AddKeyInternal, ObjectGuid));
				
				bAnyDropped = true;
			}
		}
	}

	FMovieSceneTrackEditor::EndKeying();

	return bAnyDropped ? FReply::Handled() : FReply::Unhandled();
}


/* FCameraCutTrackEditor implementation
 *****************************************************************************/

FKeyPropertyResult FCameraCutTrackEditor::AddKeyInternal( FFrameNumber KeyTime, const FGuid ObjectGuid )
{
	FKeyPropertyResult KeyPropertyResult;

	UMovieSceneCameraCutTrack* CameraCutTrack = FindOrCreateCameraCutTrack();
	const TArray<UMovieSceneSection*>& AllSections = CameraCutTrack->GetAllSections();

	UMovieSceneCameraCutSection* NewSection = CameraCutTrack->AddNewCameraCut(UE::MovieScene::FRelativeObjectBindingID(ObjectGuid), KeyTime);
	KeyPropertyResult.bTrackModified = true;
	KeyPropertyResult.SectionsCreated.Add(NewSection);

	GetSequencer()->EmptySelection();
	GetSequencer()->SelectSection(NewSection);
	GetSequencer()->ThrobSectionSelection();

	return KeyPropertyResult;
}


UMovieSceneCameraCutTrack* FCameraCutTrackEditor::FindOrCreateCameraCutTrack()
{
	UMovieScene* FocusedMovieScene = GetFocusedMovieScene();
	if (FocusedMovieScene->IsReadOnly())
	{
		return nullptr;
	}

	UMovieSceneTrack* CameraCutTrack = FocusedMovieScene->GetCameraCutTrack();

	if (CameraCutTrack == nullptr)
	{
		const FScopedTransaction Transaction(LOCTEXT("AddCameraCutTrack_Transaction", "Add Camera Cut Track"));
		FocusedMovieScene->Modify();
		
		CameraCutTrack = FocusedMovieScene->AddCameraCutTrack(UMovieSceneCameraCutTrack::StaticClass());
	}

	return CastChecked<UMovieSceneCameraCutTrack>(CameraCutTrack);
}


/* FCameraCutTrackEditor callbacks
 *****************************************************************************/

bool FCameraCutTrackEditor::HandleAddCameraCutTrackMenuEntryCanExecute() const
{
	UMovieScene* FocusedMovieScene = GetFocusedMovieScene();

	return ((FocusedMovieScene != nullptr) && (FocusedMovieScene->GetCameraCutTrack() == nullptr));
}


void FCameraCutTrackEditor::HandleAddCameraCutTrackMenuEntryExecute()
{
	UMovieSceneCameraCutTrack* CameraCutTrack = FindOrCreateCameraCutTrack();

	if (CameraCutTrack)
	{
		if (GetSequencer().IsValid())
		{
			GetSequencer()->OnAddTrack(CameraCutTrack, FGuid());
		}
	}
}

bool FCameraCutTrackEditor::IsCameraPickable(const AActor* const PickableActor)
{
	if (PickableActor->IsListedInSceneOutliner() &&
		!FActorEditorUtils::IsABuilderBrush(PickableActor) &&
		!PickableActor->IsA( AWorldSettings::StaticClass() ) &&
		IsValid(PickableActor))
	{	
		UCameraComponent* CameraComponent = MovieSceneHelpers::CameraComponentFromActor(PickableActor);
		if (CameraComponent)	
		{
			return true;
		}
	}
	return false;
}

TSharedRef<SWidget> FCameraCutTrackEditor::HandleAddCameraCutComboButtonGetMenuContent()
{
	FMenuBuilder MenuBuilder(true, nullptr);

	auto CreateNewCamera =
		[this](FMenuBuilder& SubMenuBuilder)
		{
			FSceneOutlinerInitializationOptions InitOptions;
			{
				InitOptions.bShowHeaderRow = false;
				InitOptions.bFocusSearchBoxWhenOpened = true;
				InitOptions.bShowTransient = true;
				InitOptions.bShowCreateNewFolder = false;
				// Only want the actor label column
				InitOptions.ColumnMap.Add(FSceneOutlinerBuiltInColumnTypes::Label(), FSceneOutlinerColumnInfo(ESceneOutlinerColumnVisibility::Visible, 0));

				// Only display Actors that we can attach too
				InitOptions.Filters->AddFilterPredicate<FActorTreeItem>(FActorTreeItem::FFilterPredicate::CreateRaw(this, &FCameraCutTrackEditor::IsCameraPickable));
			}		

			// Actor selector to allow the user to choose a parent actor
			FSceneOutlinerModule& SceneOutlinerModule = FModuleManager::LoadModuleChecked<FSceneOutlinerModule>( "SceneOutliner" );

			TSharedRef< SWidget > MenuWidget = 
				SNew(SHorizontalBox)

				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SBox)
					.MaxDesiredHeight(400.0f)
					.WidthOverride(300.0f)
					[
						SceneOutlinerModule.CreateActorPicker(
							InitOptions,
							FOnActorPicked::CreateSP(this, &FCameraCutTrackEditor::HandleAddCameraCutComboButtonMenuEntryExecute )
							)
					]
				];
			SubMenuBuilder.AddWidget(MenuWidget, FText::GetEmpty(), false);
		};

	TSharedPtr<ISequencer> SequencerPtr = GetSequencer();

	// Always recreate the binding picker to ensure we have the correct sequence ID
	BindingIDPicker = MakeShared<FTrackEditorBindingIDPicker>(SequencerPtr->GetFocusedTemplateID(), SequencerPtr);
	BindingIDPicker->OnBindingPicked().AddRaw(this, &FCameraCutTrackEditor::CreateNewSectionFromBinding);

	FText ExistingBindingText = LOCTEXT("ExistingBinding", "Existing Binding");
	FText NewBindingText = LOCTEXT("NewBinding", "New Binding");

	const bool bHasExistingBindings = !BindingIDPicker->IsEmpty();
	if (bHasExistingBindings)
	{
		MenuBuilder.AddSubMenu(
			NewBindingText,
			LOCTEXT("NewBinding_Tip", "Add a new camera cut by creating a new binding to an object in the world."),
			FNewMenuDelegate::CreateLambda(CreateNewCamera)
		);

		MenuBuilder.BeginSection(NAME_None, ExistingBindingText);
		{
			BindingIDPicker->GetPickerMenu(MenuBuilder);
		}
		MenuBuilder.EndSection();
	}
	else
	{
		MenuBuilder.BeginSection(NAME_None, NewBindingText);
		{
			CreateNewCamera(MenuBuilder);
		}
		MenuBuilder.EndSection();
	}
	
	return MenuBuilder.MakeWidget();
}


void FCameraCutTrackEditor::CreateNewSectionFromBinding(FMovieSceneObjectBindingID InBindingID)
{
	auto CreateNewSection = [this, InBindingID](FFrameNumber KeyTime)
	{
		FKeyPropertyResult KeyPropertyResult;

		UMovieSceneCameraCutSection* NewSection = FindOrCreateCameraCutTrack()->AddNewCameraCut(InBindingID, KeyTime);
		KeyPropertyResult.bTrackModified = true;
		KeyPropertyResult.SectionsCreated.Add(NewSection);

		GetSequencer()->EmptySelection();
		GetSequencer()->SelectSection(NewSection);
		GetSequencer()->ThrobSectionSelection();
		
		return KeyPropertyResult;
	};

	AnimatablePropertyChanged(FOnKeyProperty::CreateLambda(CreateNewSection));
}


void FCameraCutTrackEditor::HandleAddCameraCutComboButtonMenuEntryExecute(AActor* Camera)
{
	FGuid ObjectGuid = FindOrCreateHandleToObject(Camera).Handle;

	if (ObjectGuid.IsValid())
	{
		AnimatablePropertyChanged(FOnKeyProperty::CreateRaw(this, &FCameraCutTrackEditor::AddKeyInternal, ObjectGuid));
	}
}

ECheckBoxState FCameraCutTrackEditor::IsCameraLocked() const
{
	if (GetSequencer()->IsPerspectiveViewportCameraCutEnabled())
	{
		return ECheckBoxState::Checked;
	}
	else
	{
		return ECheckBoxState::Unchecked;
	}
}


void FCameraCutTrackEditor::OnLockCameraClicked(ECheckBoxState CheckBoxState)
{
	if (CheckBoxState == ECheckBoxState::Checked)
	{
		for(FLevelEditorViewportClient* LevelVC : GEditor->GetLevelViewportClients())
		{
			if (LevelVC && LevelVC->AllowsCinematicControl() && LevelVC->GetViewMode() != VMI_Unknown)
			{
				LevelVC->SetActorLock(nullptr);
				LevelVC->bLockedCameraView = false;
				LevelVC->UpdateViewForLockedActor();
				LevelVC->Invalidate();
			}
		}
		GetSequencer()->SetPerspectiveViewportCameraCutEnabled(true);
	}
	else
	{
		GetSequencer()->UpdateCameraCut(nullptr, EMovieSceneCameraCutParams());
		GetSequencer()->SetPerspectiveViewportCameraCutEnabled(false);
	}

	GetSequencer()->ForceEvaluate();
}

void FCameraCutTrackEditor::ToggleLockCamera()
{
	OnLockCameraClicked(IsCameraLocked() == ECheckBoxState::Checked ?  ECheckBoxState::Unchecked :  ECheckBoxState::Checked);
}

FText FCameraCutTrackEditor::GetLockCameraToolTip() const
{
	const TSharedRef<const FInputChord> FirstActiveChord = FCameraCutTrackCommands::Get().ToggleLockCamera->GetFirstValidChord();

	FText Tooltip = IsCameraLocked() == ECheckBoxState::Checked ?
		LOCTEXT("UnlockCamera", "Unlock Viewport from Camera Cuts") :
		LOCTEXT("LockCamera", "Lock Viewport to Camera Cuts");
	
	if (FirstActiveChord->IsValidChord())
	{
		return FText::Join(FText::FromString(TEXT(" ")), Tooltip, FirstActiveChord->GetInputText());
	}
	return Tooltip;
}

#undef LOCTEXT_NAMESPACE
