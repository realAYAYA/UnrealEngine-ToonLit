// Copyright Epic Games, Inc. All Rights Reserved.

#include "LevelSequenceEditorSubsystem.h"
#include "MVVM/ViewModels/SequencerEditorViewModel.h"
#include "Scripting/SequencerModuleScriptingLayer.h"
#include "SequencerCurveEditorObject.h"
#include "Evaluation/MovieScenePlayback.h"
#include "ISequencerModule.h"
#include "Framework/Commands/UICommandList.h"
#include "LevelSequence.h"
#include "ISceneOutliner.h"
#include "LevelSequenceEditorCommands.h"
#include "MovieScenePossessable.h"
#include "SequencerSettings.h"
#include "MovieScene.h"
#include "MovieSceneSpawnable.h"
#include "SequencerUtilities.h"
#include "Sections/MovieScene3DConstraintSection.h"
#include "Selection.h"
#include "Tracks/MovieScene3DTransformTrack.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "Tracks/MovieScene3DConstraintTrack.h"
#include "Tracks/MovieSceneCameraShakeTrack.h"

#include "ActorTreeItem.h"
#include "Editor.h"
#include "PropertyEditorModule.h"
#include "Widgets/SWindow.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SSpacer.h"
#include "Modules/ModuleManager.h"

#include "Camera/CameraComponent.h"
#include "ClassViewerFilter.h"
#include "ClassViewerModule.h"
#include "Components/SkeletalMeshComponent.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Modules/ModuleManager.h"
#include "SceneOutlinerModule.h"
#include "ScopedTransaction.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "BakingAnimationKeySettings.h"
#include "IStructureDetailsView.h"
#include "Widgets/SWidget.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/NumericTypeInterface.h"
#include "FrameNumberDetailsCustomization.h"
#include "MovieSceneToolHelpers.h"
#include "ActorForWorldTransforms.h"
#include "KeyParams.h"
#include "Sections/MovieSceneBindingLifetimeSection.h"
#include "Tracks/MovieSceneBindingLifetimeTrack.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LevelSequenceEditorSubsystem)

DEFINE_LOG_CATEGORY(LogLevelSequenceEditor);

#define LOCTEXT_NAMESPACE "LevelSequenceEditor"

void ULevelSequenceEditorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	UE_LOG(LogLevelSequenceEditor, Log, TEXT("LevelSequenceEditor subsystem initialized."));

	ISequencerModule& SequencerModule = FModuleManager::Get().LoadModuleChecked<ISequencerModule>("Sequencer");
	OnSequencerCreatedHandle = SequencerModule.RegisterOnSequencerCreated(FOnSequencerCreated::FDelegate::CreateUObject(this, &ULevelSequenceEditorSubsystem::OnSequencerCreated));

	auto AreActorsSelected = [this]{
		TArray<AActor*> SelectedActors;
		GEditor->GetSelectedActors()->GetSelectedObjects<AActor>(SelectedActors); 
		return SelectedActors.Num() > 0;
	};

	auto AreMovieSceneSectionsSelected = [this](const int32 MinSections = 1) {
		const TSharedPtr<ISequencer> Sequencer = GetActiveSequencer();
		if (!Sequencer)
		{
			return false;
		}

		TArray<UMovieSceneSection*> SelectedSections;
		Sequencer->GetSelectedSections(SelectedSections);
		return (SelectedSections.Num() >= MinSections);
	};

	/* Commands for this subsystem */
	CommandList = MakeShareable(new FUICommandList);

	CommandList->MapAction(FLevelSequenceEditorCommands::Get().SnapSectionsToTimelineUsingSourceTimecode,
		FExecuteAction::CreateUObject(this, &ULevelSequenceEditorSubsystem::SnapSectionsToTimelineUsingSourceTimecodeInternal),
		FCanExecuteAction::CreateLambda(AreMovieSceneSectionsSelected)
	);

	CommandList->MapAction(FLevelSequenceEditorCommands::Get().SyncSectionsUsingSourceTimecode,
		FExecuteAction::CreateUObject(this, &ULevelSequenceEditorSubsystem::SyncSectionsUsingSourceTimecodeInternal),
		FCanExecuteAction::CreateLambda(AreMovieSceneSectionsSelected, 2)
	);

	CommandList->MapAction(FLevelSequenceEditorCommands::Get().BakeTransform,
		FExecuteAction::CreateUObject(this, &ULevelSequenceEditorSubsystem::BakeTransformInternal)
	);

	CommandList->MapAction(FLevelSequenceEditorCommands::Get().FixActorReferences,
		FExecuteAction::CreateUObject(this, &ULevelSequenceEditorSubsystem::FixActorReferences)
	);

	CommandList->MapAction(FLevelSequenceEditorCommands::Get().AddActorsToBinding,
		FExecuteAction::CreateUObject(this, &ULevelSequenceEditorSubsystem::AddActorsToBindingInternal),
		FCanExecuteAction::CreateLambda(AreActorsSelected));

	CommandList->MapAction(FLevelSequenceEditorCommands::Get().ReplaceBindingWithActors,
		FExecuteAction::CreateUObject(this, &ULevelSequenceEditorSubsystem::ReplaceBindingWithActorsInternal),
		FCanExecuteAction::CreateLambda(AreActorsSelected));

	CommandList->MapAction(FLevelSequenceEditorCommands::Get().RemoveActorsFromBinding,
		FExecuteAction::CreateUObject(this, &ULevelSequenceEditorSubsystem::RemoveActorsFromBindingInternal),
		FCanExecuteAction::CreateLambda(AreActorsSelected));

	CommandList->MapAction(FLevelSequenceEditorCommands::Get().RemoveAllBindings,
		FExecuteAction::CreateUObject(this, &ULevelSequenceEditorSubsystem::RemoveAllBindingsInternal));

	CommandList->MapAction(FLevelSequenceEditorCommands::Get().RemoveInvalidBindings,
		FExecuteAction::CreateUObject(this, &ULevelSequenceEditorSubsystem::RemoveInvalidBindingsInternal));

	/* Menu extenders */
	TransformMenuExtender = MakeShareable(new FExtender);
	TransformMenuExtender->AddMenuExtension("Transform", EExtensionHook::After, CommandList, FMenuExtensionDelegate::CreateLambda([this](FMenuBuilder& MenuBuilder) {
		// Only add menu entries where the focused sequence is a ULevelSequence
		if (!GetActiveSequencer())
		{
			return;
		}
		
		MenuBuilder.AddMenuEntry(FLevelSequenceEditorCommands::Get().SnapSectionsToTimelineUsingSourceTimecode);
		MenuBuilder.AddMenuEntry(FLevelSequenceEditorCommands::Get().SyncSectionsUsingSourceTimecode);
		MenuBuilder.AddMenuEntry(FLevelSequenceEditorCommands::Get().BakeTransform);
		}));

	SequencerModule.GetActionsMenuExtensibilityManager()->AddExtender(TransformMenuExtender);

	FixActorReferencesMenuExtender = MakeShareable(new FExtender);
	FixActorReferencesMenuExtender->AddMenuExtension("Bindings", EExtensionHook::First, CommandList, FMenuExtensionDelegate::CreateLambda([this](FMenuBuilder& MenuBuilder) {
		// Only add menu entries where the focused sequence is a ULevelSequence
		if (!GetActiveSequencer())
		{
			return;
		}
		
		MenuBuilder.AddMenuEntry(FLevelSequenceEditorCommands::Get().FixActorReferences);
		}));

	SequencerModule.GetActionsMenuExtensibilityManager()->AddExtender(FixActorReferencesMenuExtender);

	AssignActorMenuExtender = MakeShareable(new FExtender);
	AssignActorMenuExtender->AddMenuExtension("Possessable", EExtensionHook::First, CommandList, FMenuExtensionDelegate::CreateLambda([this](FMenuBuilder& MenuBuilder) {
		// Only add menu entries where the focused sequence is a ULevelSequence
		if (!GetActiveSequencer())
		{
			return;
		}
		
		FFormatNamedArguments Args;
		MenuBuilder.AddSubMenu(
			FText::Format(LOCTEXT("AssignActor", "Assign Actor"), Args),
			FText::Format(LOCTEXT("AssignActorTooltip", "Assign an actor to this track"), Args),
			FNewMenuDelegate::CreateLambda([this](FMenuBuilder& SubMenuBuilder) { AddAssignActorMenu(SubMenuBuilder); } ));
		}));

	SequencerModule.GetObjectBindingContextMenuExtensibilityManager()->AddExtender(AssignActorMenuExtender);

	// For now we have the binding properties being a separate menu. When the UX is worked out we will likely merge the AssignActor menu away.
	BindingPropertiesMenuExtender = MakeShareable(new FExtender);
	BindingPropertiesMenuExtender->AddMenuExtension("Possessable", EExtensionHook::First, CommandList, FMenuExtensionDelegate::CreateLambda([this](FMenuBuilder& MenuBuilder) {
		// Only add menu entries where the focused sequence is a ULevelSequence
		if (!GetActiveSequencer())
		{
			return;
		}
		
		FFormatNamedArguments Args;
		MenuBuilder.AddSubMenu(
			FText::Format(LOCTEXT("BindingProperties", "Binding Properties"), Args),
			FText::Format(LOCTEXT("BindingPropertiesTooltip", "Modify the actor and object bindings for this track"), Args),
			FNewMenuDelegate::CreateLambda([this](FMenuBuilder& SubMenuBuilder) { AddBindingPropertiesMenu(SubMenuBuilder); } ));
		}));

	SequencerModule.GetObjectBindingContextMenuExtensibilityManager()->AddExtender(BindingPropertiesMenuExtender);

	RebindComponentMenuExtender = MakeShareable(new FExtender);
	RebindComponentMenuExtender->AddMenuExtension("Possessable", EExtensionHook::First, CommandList, FMenuExtensionDelegate::CreateLambda([this](FMenuBuilder& MenuBuilder) {
		// Only add menu entries where the focused sequence is a ULevelSequence
		if (!GetActiveSequencer())
		{
			return;
		}
		
		TArray<FName> ComponentNames;
		GetRebindComponentNames(ComponentNames);
		if (ComponentNames.Num() > 0)
		{
			FFormatNamedArguments Args;
			MenuBuilder.AddSubMenu(
				FText::Format(LOCTEXT("RebindComponent", "Rebind Component"), Args),
				FText::Format(LOCTEXT("RebindComponentTooltip", "Rebind component by moving the tracks from one component to another component."), Args),
				FNewMenuDelegate::CreateLambda([this](FMenuBuilder& SubMenuBuilder) { RebindComponentMenu(SubMenuBuilder); } ));
		}
	}));

	SequencerModule.GetObjectBindingContextMenuExtensibilityManager()->AddExtender(RebindComponentMenuExtender);
}

void ULevelSequenceEditorSubsystem::Deinitialize()
{
	UE_LOG(LogLevelSequenceEditor, Log, TEXT("LevelSequenceEditor subsystem deinitialized."));

	ISequencerModule* SequencerModulePtr = FModuleManager::Get().GetModulePtr<ISequencerModule>("Sequencer");
	if (SequencerModulePtr)
	{
		SequencerModulePtr->UnregisterOnSequencerCreated(OnSequencerCreatedHandle);
	}

}

void ULevelSequenceEditorSubsystem::OnSequencerCreated(TSharedRef<ISequencer> InSequencer)
{
	UE_LOG(LogLevelSequenceEditor, VeryVerbose, TEXT("ULevelSequenceEditorSubsystem::OnSequencerCreated"));

	Sequencers.Add(TWeakPtr<ISequencer>(InSequencer));
}

TSharedPtr<ISequencer> ULevelSequenceEditorSubsystem::GetActiveSequencer()
{
	for (TWeakPtr<ISequencer> Ptr : Sequencers)
	{
		if (Ptr.IsValid())
		{
			UMovieSceneSequence* Sequence = Ptr.Pin()->GetFocusedMovieSceneSequence();
			if (Sequence && Sequence->IsA<ULevelSequence>())
			{
				return Ptr.Pin();
			}
		}
	}

	return nullptr;
}

USequencerModuleScriptingLayer* ULevelSequenceEditorSubsystem::GetScriptingLayer()
{
	TSharedPtr<ISequencer> Sequencer = GetActiveSequencer();
	if (Sequencer)
	{
		return Cast<USequencerModuleScriptingLayer>(Sequencer->GetViewModel()->GetScriptingLayer());
	}
	return nullptr;
}

USequencerCurveEditorObject* ULevelSequenceEditorSubsystem::GetCurveEditor()
{
	TObjectPtr<USequencerCurveEditorObject> CurveEditorObject;
	TSharedPtr<ISequencer> Sequencer = GetActiveSequencer();
	if (Sequencer)
	{
		TObjectPtr<USequencerCurveEditorObject> *ExistingCurveEditorObject = CurveEditorObjects.Find(Sequencer);
		if (ExistingCurveEditorObject)
		{
			CurveEditorObject = *ExistingCurveEditorObject;
		}
		else
		{
			CurveEditorObject = NewObject<USequencerCurveEditorObject>(this);
			CurveEditorObject->SetSequencer(Sequencer);
			CurveEditorObjects.Add(Sequencer, CurveEditorObject);
			CurveEditorArray.Add(CurveEditorObject);
		}
	}
	return CurveEditorObject;
}

TArray<FMovieSceneBindingProxy> ULevelSequenceEditorSubsystem::AddActors(const TArray<AActor*>& InActors)
{
	TArray<FMovieSceneBindingProxy> BindingProxies;

	TSharedPtr<ISequencer> Sequencer = GetActiveSequencer();
	if (Sequencer == nullptr)
	{
		return BindingProxies;
	}

	UMovieSceneSequence* Sequence = Sequencer->GetFocusedMovieSceneSequence();
	if (!Sequence)
	{
		return BindingProxies;
	}

	TArray<TWeakObjectPtr<AActor> > Actors;
	for (AActor* Actor : InActors)
	{
		Actors.Add(Actor);
	}

	TArray<FGuid> Guids = FSequencerUtilities::AddActors(Sequencer.ToSharedRef(), Actors);
	
	for (const FGuid& Guid : Guids)
	{
		BindingProxies.Add(FMovieSceneBindingProxy(Guid, Sequence));
	}

	return BindingProxies;
}

FMovieSceneBindingProxy ULevelSequenceEditorSubsystem::CreateCamera(bool bSpawnable, ACineCameraActor*& OutActor)
{
	TSharedPtr<ISequencer> Sequencer = GetActiveSequencer();
	if (Sequencer == nullptr)
	{
		return FMovieSceneBindingProxy();
	}

	UMovieSceneSequence* Sequence = Sequencer->GetFocusedMovieSceneSequence();
	if (!Sequence)
	{
		return FMovieSceneBindingProxy();
	}

	FGuid Guid = FSequencerUtilities::CreateCamera(Sequencer.ToSharedRef(), bSpawnable, OutActor);

	return FMovieSceneBindingProxy(Guid, Sequence);
}

TArray<FMovieSceneBindingProxy> ULevelSequenceEditorSubsystem::ConvertToSpawnable(const FMovieSceneBindingProxy& ObjectBinding)
{
	TArray<FMovieSceneBindingProxy> SpawnableProxies;

	TSharedPtr<ISequencer> Sequencer = GetActiveSequencer();
	if (Sequencer == nullptr)
	{
		return SpawnableProxies;
	}

	UMovieSceneSequence* Sequence = Sequencer->GetFocusedMovieSceneSequence();
	if (!Sequence)
	{
		return SpawnableProxies;
	}

	TArray<FMovieSceneSpawnable*> Spawnables = FSequencerUtilities::ConvertToSpawnable(Sequencer.ToSharedRef(), ObjectBinding.BindingID);
	for (FMovieSceneSpawnable* Spawnable : Spawnables)
	{
		if (Spawnable)
		{
			SpawnableProxies.Add(FMovieSceneBindingProxy(Spawnable->GetGuid(), Sequence));
		}
	}

	return SpawnableProxies;
}

FMovieSceneBindingProxy ULevelSequenceEditorSubsystem::ConvertToPossessable(const FMovieSceneBindingProxy& ObjectBinding)
{
	FMovieSceneBindingProxy PossessableProxy;

	TSharedPtr<ISequencer> Sequencer = GetActiveSequencer();
	if (Sequencer == nullptr)
	{
		return PossessableProxy;
	}

	UMovieSceneSequence* Sequence = Sequencer->GetFocusedMovieSceneSequence();
	if (!Sequence)
	{
		return PossessableProxy;
	}

	if (FMovieScenePossessable* Possessable = FSequencerUtilities::ConvertToPossessable(Sequencer.ToSharedRef(), ObjectBinding.BindingID))
	{
		PossessableProxy = FMovieSceneBindingProxy(Possessable->GetGuid(), Sequence);
	}

	return PossessableProxy;
}

void ULevelSequenceEditorSubsystem::CopyFolders(const TArray<UMovieSceneFolder*>& Folders, FString& ExportedText)
{
	FSequencerUtilities::CopyFolders(Folders, ExportedText);

	FPlatformApplicationMisc::ClipboardCopy(*ExportedText);
}

bool ULevelSequenceEditorSubsystem::PasteFolders(const FString& InTextToImport, FMovieScenePasteFoldersParams PasteFoldersParams, TArray<UMovieSceneFolder*>& OutFolders)
{
	FString TextToImport = InTextToImport;
	if (TextToImport.IsEmpty())
	{
		FPlatformApplicationMisc::ClipboardPaste(TextToImport);
	}

	TArray<FNotificationInfo> PasteErrors;
	if (!FSequencerUtilities::PasteFolders(TextToImport, PasteFoldersParams, OutFolders, PasteErrors))
	{
		for (FNotificationInfo PasteError : PasteErrors)
		{
			UE_LOG(LogLevelSequenceEditor, Error, TEXT("%s"), *PasteError.Text.Get().ToString());
		}
		return false;
	}

	return true;
}

void ULevelSequenceEditorSubsystem::CopySections(const TArray<UMovieSceneSection*>& Sections, FString& ExportedText)
{
	FSequencerUtilities::CopySections(Sections, ExportedText);

	FPlatformApplicationMisc::ClipboardCopy(*ExportedText);
}

bool ULevelSequenceEditorSubsystem::PasteSections(const FString& InTextToImport, FMovieScenePasteSectionsParams PasteSectionsParams, TArray<UMovieSceneSection*>& OutSections)
{
	FString TextToImport = InTextToImport;
	if (TextToImport.IsEmpty())
	{
		FPlatformApplicationMisc::ClipboardPaste(TextToImport);
	}

	TArray<FNotificationInfo> PasteErrors;
	if (!FSequencerUtilities::PasteSections(TextToImport, PasteSectionsParams, OutSections, PasteErrors))
	{
		for (FNotificationInfo PasteError : PasteErrors)
		{
			UE_LOG(LogLevelSequenceEditor, Error, TEXT("%s"), *PasteError.Text.Get().ToString());
		}
		return false;
	}

	return true;
}

void ULevelSequenceEditorSubsystem::CopyTracks(const TArray<UMovieSceneTrack*>& Tracks, FString& ExportedText)
{
	TArray<UMovieSceneFolder*> Folders;
	FSequencerUtilities::CopyTracks(Tracks, Folders, ExportedText);
	FPlatformApplicationMisc::ClipboardCopy(*ExportedText);
}

bool ULevelSequenceEditorSubsystem::PasteTracks(const FString& InTextToImport, FMovieScenePasteTracksParams PasteTracksParams, TArray<UMovieSceneTrack*>& OutTracks)
{
	FString TextToImport = InTextToImport;
	if (TextToImport.IsEmpty())
	{
		FPlatformApplicationMisc::ClipboardPaste(TextToImport);
	}

	TArray<FNotificationInfo> PasteErrors;
	if (!FSequencerUtilities::PasteTracks(TextToImport, PasteTracksParams, OutTracks, PasteErrors))
	{
		for (FNotificationInfo PasteError : PasteErrors)
		{
			UE_LOG(LogLevelSequenceEditor, Error, TEXT("%s"), *PasteError.Text.Get().ToString());
		}
		return false;
	}

	return true;
}

void ULevelSequenceEditorSubsystem::CopyBindings(const TArray<FMovieSceneBindingProxy>& Bindings, FString& ExportedText)
{
	TSharedPtr<ISequencer> Sequencer = GetActiveSequencer();
	if (Sequencer == nullptr)
	{
		return;
	}

	TArray<UMovieSceneFolder*> Folders;
	FSequencerUtilities::CopyBindings(Sequencer.ToSharedRef(), Bindings, Folders, ExportedText);
	FPlatformApplicationMisc::ClipboardCopy(*ExportedText);
}

bool ULevelSequenceEditorSubsystem::PasteBindings(const FString& InTextToImport, FMovieScenePasteBindingsParams PasteBindingsParams, TArray<FMovieSceneBindingProxy>& OutObjectBindings)
{
	TSharedPtr<ISequencer> Sequencer = GetActiveSequencer();
	if (Sequencer == nullptr)
	{
		return false;
	}

	FString TextToImport = InTextToImport;
	if (TextToImport.IsEmpty())
	{
		FPlatformApplicationMisc::ClipboardPaste(TextToImport);
	}

	TArray<FNotificationInfo> PasteErrors;
	if (!FSequencerUtilities::PasteBindings(TextToImport, Sequencer.ToSharedRef(), PasteBindingsParams, OutObjectBindings, PasteErrors))
	{
		for (FNotificationInfo PasteError : PasteErrors)
		{
			UE_LOG(LogLevelSequenceEditor, Error, TEXT("%s"), *PasteError.Text.Get().ToString());
		}
		return false;
	}

	return true;
}

void ULevelSequenceEditorSubsystem::SnapSectionsToTimelineUsingSourceTimecodeInternal()
{
	const TSharedPtr<ISequencer> Sequencer = GetActiveSequencer();
	if (!Sequencer)
	{
		return;
	}

	const UMovieScene* FocusedMovieScene = Sequencer->GetFocusedMovieSceneSequence()->GetMovieScene();
	if (!FocusedMovieScene)
	{
		return;
	}

	if (FocusedMovieScene->IsReadOnly())
	{
		FSequencerUtilities::ShowReadOnlyError();
		return;
	}

	TArray<UMovieSceneSection*> Sections;
	Sequencer->GetSelectedSections(Sections);
	if (Sections.IsEmpty())
	{
		return;
	}

	SnapSectionsToTimelineUsingSourceTimecode(Sections);
}

void ULevelSequenceEditorSubsystem::SnapSectionsToTimelineUsingSourceTimecode(const TArray<UMovieSceneSection*>& Sections)
{
	const TSharedPtr<ISequencer> Sequencer = GetActiveSequencer();
	if (!Sequencer)
	{
		return;
	}

	const UMovieScene* FocusedMovieScene = Sequencer->GetFocusedMovieSceneSequence()->GetMovieScene();
	if (!FocusedMovieScene)
	{
		return;
	}

	if (FocusedMovieScene->IsReadOnly())
	{
		FSequencerUtilities::ShowReadOnlyError();
		return;
	}

	FScopedTransaction SnapSectionsToTimelineUsingSourceTimecodeTransaction(LOCTEXT("SnapSectionsToTimelineUsingSourceTimecode_Transaction", "Snap Sections to Timeline using Source Timecode"));
	bool bAnythingChanged = false;

	const FFrameRate TickResolution = Sequencer->GetFocusedTickResolution();
	const FFrameRate DisplayRate = Sequencer->GetFocusedDisplayRate();

	for (UMovieSceneSection* Section : Sections)
	{
		if (!Section || !Section->HasStartFrame())
		{
			continue;
		}

		const FTimecode SectionSourceTimecode = Section->TimecodeSource.Timecode;
		if (SectionSourceTimecode == FTimecode())
		{
			// Do not move sections with default values for source timecode.
			continue;
		}

		const FFrameNumber SectionSourceStartFrameNumber = SectionSourceTimecode.ToFrameNumber(DisplayRate);

		// Account for any trimming at the start of the section when computing the
		// target frame number to move this section to.
		const FFrameNumber SectionOffsetFrames = Section->GetOffsetTime().Get(FFrameTime()).FloorToFrame();
		const FFrameNumber TargetFrameNumber = SectionSourceStartFrameNumber + SectionOffsetFrames;

		const FFrameNumber SectionCurrentStartFrameNumber = Section->GetInclusiveStartFrame();

		const FFrameNumber Delta = -(SectionCurrentStartFrameNumber - ConvertFrameTime(TargetFrameNumber, DisplayRate, TickResolution).GetFrame().Value);

		Section->MoveSection(Delta);

		bAnythingChanged |= (Delta.Value != 0);
	}

	if (bAnythingChanged)
	{
		Sequencer->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::TrackValueChanged);
	}
}

void ULevelSequenceEditorSubsystem::SyncSectionsUsingSourceTimecodeInternal()
{
	const TSharedPtr<ISequencer> Sequencer = GetActiveSequencer();
	if (!Sequencer)
	{
		return;
	}

	const UMovieScene* FocusedMovieScene = Sequencer->GetFocusedMovieSceneSequence()->GetMovieScene();
	if (!FocusedMovieScene)
	{
		return;
	}

	if (FocusedMovieScene->IsReadOnly())
	{
		FSequencerUtilities::ShowReadOnlyError();
		return;
	}

	TArray<UMovieSceneSection*> Sections;
	Sequencer->GetSelectedSections(Sections);
	if (Sections.Num() < 2)
	{
		return;
	}

	SyncSectionsUsingSourceTimecode(Sections);
}

void ULevelSequenceEditorSubsystem::SyncSectionsUsingSourceTimecode(const TArray<UMovieSceneSection*>& Sections)
{
	const TSharedPtr<ISequencer> Sequencer = GetActiveSequencer();
	if (!Sequencer)
	{
		return;
	}

	const UMovieScene* FocusedMovieScene = Sequencer->GetFocusedMovieSceneSequence()->GetMovieScene();
	if (!FocusedMovieScene)
	{
		return;
	}

	if (FocusedMovieScene->IsReadOnly())
	{
		FSequencerUtilities::ShowReadOnlyError();
		return;
	}

	// Pull out all of the valid sections that have a start frame and verify
	// we have at least two sections to sync.
	TArray<UMovieSceneSection*> SectionsToSync;
	for (UMovieSceneSection* Section : Sections)
	{
		if (Section && Section->HasStartFrame())
		{
			SectionsToSync.Add(Section);
		}
	}

	if (SectionsToSync.Num() < 2)
	{
		return;
	}

	FScopedTransaction SyncSectionsUsingSourceTimecodeTransaction(LOCTEXT("SyncSectionsUsingSourceTimecode_Transaction", "Sync Sections Using Source Timecode"));
	bool bAnythingChanged = false;

	const FFrameRate TickResolution = Sequencer->GetFocusedTickResolution();
	const FFrameRate DisplayRate = Sequencer->GetFocusedDisplayRate();

	const UMovieSceneSection* FirstSection = SectionsToSync[0];
	const FFrameNumber FirstSectionSourceTimecode = FirstSection->TimecodeSource.Timecode.ToFrameNumber(DisplayRate);

	const FFrameNumber FirstSectionCurrentStartFrame = FirstSection->GetInclusiveStartFrame();
	const FFrameNumber FirstSectionOffsetFrames = FirstSection->GetOffsetTime().Get(FFrameTime()).FloorToFrame();
	SectionsToSync.RemoveAt(0);

	for (UMovieSceneSection* Section : SectionsToSync)
	{
		const FFrameNumber SectionSourceTimecode = Section->TimecodeSource.Timecode.ToFrameNumber(DisplayRate);
		const FFrameNumber SectionCurrentStartFrame = Section->GetInclusiveStartFrame();
		const FFrameNumber SectionOffsetFrames = Section->GetOffsetTime().Get(FFrameTime()).FloorToFrame();

		const FFrameNumber TimecodeDelta = ConvertFrameTime(SectionSourceTimecode - FirstSectionSourceTimecode, DisplayRate, TickResolution).GetFrame().Value;
		const FFrameNumber CurrentDelta = (SectionCurrentStartFrame - SectionOffsetFrames) - (FirstSectionCurrentStartFrame - FirstSectionOffsetFrames);
		const FFrameNumber Delta = -CurrentDelta + TimecodeDelta;

		Section->MoveSection(Delta);

		bAnythingChanged |= (Delta.Value != 0);
	}

	if (bAnythingChanged)
	{
		Sequencer->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::TrackValueChanged);
	}
}

//////////////////////////////////////////////////////////////
/// SBakeTransformWidget
///////////////////////////////////////////////////////////

DECLARE_DELEGATE_RetVal_OneParam(FReply, SBakeTransformOnBake, FBakingAnimationKeySettings);

/** Widget allowing baking controls from one space to another */
class  SBakeTransformWidget : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SBakeTransformWidget)
		: _Sequencer(nullptr)
	{}
		SLATE_ARGUMENT(ISequencer*, Sequencer)
		SLATE_ARGUMENT(FBakingAnimationKeySettings, Settings)
		SLATE_EVENT(SBakeTransformOnBake, OnBake)
		SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SBakeTransformWidget() override {}

	FReply OpenDialog(bool bModal = true);
	void CloseDialog();

private:

	//used for setting up the details
	TSharedPtr<TStructOnScope<FBakingAnimationKeySettings>> Settings;

	ISequencer* Sequencer;

	TWeakPtr<SWindow> DialogWindow;
	TSharedPtr<IStructureDetailsView> DetailsView;
};

void SBakeTransformWidget::Construct(const FArguments& InArgs)
{
	check(InArgs._Sequencer);
	check(InArgs._OnBake.IsBound());

	Settings = MakeShared<TStructOnScope<FBakingAnimationKeySettings>>();
	Settings->InitializeAs<FBakingAnimationKeySettings>();
	*Settings = InArgs._Settings;
	//always setting space to be parent as default, since stored space may not be available.
	Sequencer = InArgs._Sequencer;

	FStructureDetailsViewArgs StructureViewArgs;
	StructureViewArgs.bShowObjects = true;
	StructureViewArgs.bShowAssets = true;
	StructureViewArgs.bShowClasses = true;
	StructureViewArgs.bShowInterfaces = true;

	FDetailsViewArgs ViewArgs;
	ViewArgs.bAllowSearch = false;
	ViewArgs.bHideSelectionTip = false;
	ViewArgs.bShowObjectLabel = false;

	FPropertyEditorModule& PropertyEditor = FModuleManager::Get().LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));

	DetailsView = PropertyEditor.CreateStructureDetailView(ViewArgs, StructureViewArgs, TSharedPtr<FStructOnScope>());
	TSharedPtr<INumericTypeInterface<double>> NumericTypeInterface = Sequencer->GetNumericTypeInterface();
	DetailsView->GetDetailsView()->RegisterInstancedCustomPropertyTypeLayout("FrameNumber",
		FOnGetPropertyTypeCustomizationInstance::CreateLambda([=]() {return MakeShared<FFrameNumberDetailsCustomization>(NumericTypeInterface); }));
	DetailsView->SetStructureData(Settings);

	ChildSlot
		[
			SNew(SBorder)
			.Visibility(EVisibility::Visible)
		[
			SNew(SVerticalBox)

			
			+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.f, 0.f, 0.f, 0.f)
				[
					DetailsView->GetWidget().ToSharedRef()
				]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 16.f, 0.f, 16.f)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			[
				SNew(SSpacer)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.Padding(8.f, 0.f, 0.f, 0.f)
			[
				SNew(SButton)
				.HAlign(HAlign_Center)
			.ContentPadding(FAppStyle::GetMargin("StandardDialog.ContentPadding"))
			.Text(LOCTEXT("OK", "OK"))
			.OnClicked_Lambda([this, InArgs]()
				{
					FReply Reply = InArgs._OnBake.Execute( *(Settings->Get()));
					CloseDialog();
					return Reply;

				})
			]

		+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.Padding(8.f, 0.f, 16.f, 0.f)
			[
				SNew(SButton)
				.HAlign(HAlign_Center)
				.ContentPadding(FAppStyle::GetMargin("StandardDialog.ContentPadding"))
				.Text(LOCTEXT("Cancel", "Cancel"))
				.OnClicked_Lambda([this]()
				{
					CloseDialog();
					return FReply::Handled();
				})
			]
		]
	]
	];
}

class SBakeTransformDialogWindow : public SWindow
{
};

FReply SBakeTransformWidget::OpenDialog(bool bModal)
{
	check(!DialogWindow.IsValid());

	const FVector2D CursorPos = FSlateApplication::Get().GetCursorPos();

	TSharedRef<SBakeTransformDialogWindow> Window = SNew(SBakeTransformDialogWindow)
		.Title(LOCTEXT("SBakeTransformWidgetTitle", "Bake Transforms"))
		.CreateTitleBar(true)
		.Type(EWindowType::Normal)
		.SizingRule(ESizingRule::Autosized)
		.ScreenPosition(CursorPos)
		.FocusWhenFirstShown(true)
		.ActivationPolicy(EWindowActivationPolicy::FirstShown)
		[
			AsShared()
		];

	Window->SetWidgetToFocusOnActivate(AsShared());

	DialogWindow = Window;

	Window->MoveWindowTo(CursorPos);

	if (bModal)
	{
		GEditor->EditorAddModalWindow(Window);
	}
	else
	{
		FSlateApplication::Get().AddWindow(Window);
	}

	return FReply::Handled();
}

void SBakeTransformWidget::CloseDialog()
{
	if (DialogWindow.IsValid())
	{
		DialogWindow.Pin()->RequestDestroyWindow();
		DialogWindow.Reset();
	}
}

void ULevelSequenceEditorSubsystem::BakeTransformInternal()
{
	TSharedPtr<ISequencer> Sequencer = GetActiveSequencer();
	if (Sequencer == nullptr)
	{
		return;
	}

	UMovieScene* FocusedMovieScene = Sequencer->GetFocusedMovieSceneSequence()->GetMovieScene();
	if (!FocusedMovieScene)
	{
		return;
	}

	FFrameRate TickResolution = FocusedMovieScene->GetTickResolution();
	FFrameRate DisplayRate = FocusedMovieScene->GetDisplayRate();

	TArray<FGuid> ObjectBindings;
	Sequencer->GetSelectedObjects(ObjectBindings);

	TArray<FMovieSceneBindingProxy> BindingProxies;
	for (FGuid Guid : ObjectBindings)
	{
		BindingProxies.Add(FMovieSceneBindingProxy(Guid, Sequencer->GetFocusedMovieSceneSequence()));
	}

	static FBakingAnimationKeySettings Settings; //reuse the settings except for the range
	Settings.StartFrame = UE::MovieScene::DiscreteInclusiveLower(FocusedMovieScene->GetPlaybackRange());
	Settings.EndFrame = UE::MovieScene::DiscreteExclusiveUpper(FocusedMovieScene->GetPlaybackRange());

	TSharedRef<SBakeTransformWidget> BakeWidget =
		SNew(SBakeTransformWidget)
		.Settings(Settings)
		.Sequencer(Sequencer.Get())
		.OnBake_Lambda([this, &BindingProxies, Sequencer](FBakingAnimationKeySettings InSettings)
			{
				FMovieSceneScriptingParams Params;
				Params.TimeUnit = EMovieSceneTimeUnit::TickResolution;
				BakeTransformWithSettings(BindingProxies, InSettings, Params);
				return FReply::Handled();
			});

	BakeWidget->OpenDialog(true);
}

void ULevelSequenceEditorSubsystem::BakeTransform(const TArray<FMovieSceneBindingProxy>& ObjectBindings, const FFrameTime& BakeInTime, const FFrameTime& BakeOutTime, const FFrameTime& BakeInterval, const FMovieSceneScriptingParams& Params)
{
	TSharedPtr<ISequencer> Sequencer = GetActiveSequencer();
	if (Sequencer == nullptr)
	{
		return;
	}

	UMovieScene* FocusedMovieScene = Sequencer->GetFocusedMovieSceneSequence()->GetMovieScene();
	if (!FocusedMovieScene)
	{
		return;
	}

	if (FocusedMovieScene->IsReadOnly())
	{
		FSequencerUtilities::ShowReadOnlyError();
		return;
	}

	if (ObjectBindings.Num() == 0)
	{
		return;
	}

	FFrameRate TickResolution = FocusedMovieScene->GetTickResolution();
	FFrameRate DisplayRate = FocusedMovieScene->GetDisplayRate();
	FFrameTime InFrame = BakeInTime;
	FFrameTime OutFrame = BakeOutTime;
	FFrameTime Interval = BakeInterval;

	if (Params.TimeUnit == EMovieSceneTimeUnit::DisplayRate)
	{
		InFrame = ConvertFrameTime(BakeInTime, DisplayRate, TickResolution);
		OutFrame = ConvertFrameTime(BakeOutTime, DisplayRate, TickResolution);
		Interval = ConvertFrameTime(BakeInterval, DisplayRate, TickResolution);
	}

	FBakingAnimationKeySettings Settings;
	FFrameTime OneFrame = ConvertFrameTime(1, DisplayRate, TickResolution);
	//we never want subframes when baking so use frame increments
	Settings.FrameIncrement = Interval.FrameNumber.Value / OneFrame.FrameNumber.Value;
	Settings.FrameIncrement = Settings.FrameIncrement <= 0 ? 1 : Settings.FrameIncrement;
	Settings.StartFrame = InFrame.GetFrame();
	Settings.EndFrame = OutFrame.GetFrame();
	Settings.BakingKeySettings = EBakingKeySettings::AllFrames;
	FMovieSceneScriptingParams NewParams;
	NewParams.TimeUnit = EMovieSceneTimeUnit::TickResolution;
	BakeTransformWithSettings(ObjectBindings, Settings, NewParams);
}

void ULevelSequenceEditorSubsystem::CalculateFramesPerGuid(TSharedPtr<ISequencer>& Sequencer, const FBakingAnimationKeySettings& InSettings, TMap<FGuid, ULevelSequenceEditorSubsystem::FBakeData>& OutBakeDataMap,
	TSortedMap<FFrameNumber, FFrameNumber>& OutFrameMap)
{
	OutFrameMap.Reset();
	TArray<FFrameNumber> Frames;
	//we get all frames since we need to get the Actor PER FRAME in order to handle spanwables
	MovieSceneToolHelpers::CalculateFramesBetween(Sequencer->GetFocusedMovieSceneSequence()->GetMovieScene(), InSettings.StartFrame, InSettings.EndFrame, InSettings.FrameIncrement, Frames);
	if (InSettings.BakingKeySettings == EBakingKeySettings::AllFrames)
	{
		for (FFrameNumber& Frame : Frames)
		{
			OutFrameMap.Add(Frame,Frame);
		}
		for (TPair<FGuid, FBakeData>& BakeData : OutBakeDataMap)
		{
			BakeData.Value.KeyTimes.Reset();
			BakeData.Value.KeyTimes = OutFrameMap;
		}
	}
	else
	{
		for (TPair<FGuid, FBakeData>& BakeData : OutBakeDataMap)
		{
			FActorForWorldTransforms ActorForWorldTransforms;
			FGuid Guid = BakeData.Key;

			for (TWeakObjectPtr<> RuntimeObject : Sequencer->FindObjectsInCurrentSequence(Guid))
			{
				UActorComponent* ActorComponent = nullptr;
				AActor* Actor = Cast<AActor>(RuntimeObject.Get());
				if (!Actor)
				{
					ActorComponent = Cast<UActorComponent>(RuntimeObject.Get());
					if (ActorComponent)
					{
						Actor = ActorComponent->GetOwner();
					}
				}
				ActorForWorldTransforms.Actor = Actor;
				ActorForWorldTransforms.Component = Cast<USceneComponent>(ActorComponent);
				BakeData.Value.KeyTimes.Reset();
				MovieSceneToolHelpers::GetActorsAndParentsKeyFrames(Sequencer.Get(), ActorForWorldTransforms,
					InSettings.StartFrame, InSettings.EndFrame, BakeData.Value.KeyTimes);
				OutFrameMap.Append(BakeData.Value.KeyTimes);
			}
		}
	}
}

bool ULevelSequenceEditorSubsystem::BakeTransformWithSettings(const TArray<FMovieSceneBindingProxy>& ObjectBindings, const FBakingAnimationKeySettings& InSettings, const FMovieSceneScriptingParams& Params)
{
	TSharedPtr<ISequencer> Sequencer = GetActiveSequencer();
	if (Sequencer == nullptr)
	{
		UE_LOG(LogLevelSequenceEditor, Warning, TEXT("Bake Transform failed."));
		return false;
	}

	UMovieScene* FocusedMovieScene = Sequencer->GetFocusedMovieSceneSequence()->GetMovieScene();
	if (!FocusedMovieScene)
	{
		UE_LOG(LogLevelSequenceEditor, Warning, TEXT("Bake Transform failed."));
		return false;
	}

	if (FocusedMovieScene->IsReadOnly())
	{
		UE_LOG(LogLevelSequenceEditor, Warning, TEXT("Bake Transform failed."));
		FSequencerUtilities::ShowReadOnlyError();
		return false;
	}

	if (ObjectBindings.Num() == 0)
	{
		UE_LOG(LogLevelSequenceEditor, Warning, TEXT("Bake Transform failed."));
		return false;
	}

	FScopedTransaction BakeTransform(LOCTEXT("BakeTransform", "Bake Transform"));

	FocusedMovieScene->Modify();

	FQualifiedFrameTime ResetTime = Sequencer->GetLocalTime();

	FFrameRate TickResolution = FocusedMovieScene->GetTickResolution();
	FFrameRate DisplayRate = FocusedMovieScene->GetDisplayRate();

	FBakingAnimationKeySettings SettingsInTick = InSettings;
	
	if (Params.TimeUnit == EMovieSceneTimeUnit::DisplayRate)
	{
		SettingsInTick.StartFrame = ConvertFrameTime(SettingsInTick.StartFrame, DisplayRate, TickResolution).GetFrame();
		SettingsInTick.EndFrame = ConvertFrameTime(SettingsInTick.EndFrame, DisplayRate, TickResolution).GetFrame();
	}

	TSortedMap<FFrameNumber, FFrameNumber> TotalFrameMap;
	TMap<FGuid, FBakeData> BakeDataMap;
	for (const FMovieSceneBindingProxy& ObjectBinding : ObjectBindings)
	{
		BakeDataMap.Add(ObjectBinding.BindingID);
	}
	CalculateFramesPerGuid(Sequencer, SettingsInTick, BakeDataMap, TotalFrameMap);

	FMovieSceneSequenceTransform RootToLocalTransform = Sequencer->GetFocusedMovieSceneSequenceTransform();
	
	TArray<FFrameNumber> AllFrames;
	TotalFrameMap.GenerateKeyArray(AllFrames);

	for (FFrameNumber KeyTime: AllFrames)
	{
		FMovieSceneEvaluationRange Range(KeyTime * RootToLocalTransform.InverseNoLooping(), TickResolution);

		Sequencer->SetGlobalTime(Range.GetTime());

		for (const FMovieSceneBindingProxy& ObjectBinding : ObjectBindings)
		{
			FGuid Guid = ObjectBinding.BindingID;

			for (TWeakObjectPtr<> RuntimeObject : Sequencer->FindObjectsInCurrentSequence(Guid) )
			{
				const FFrameNumber* Number = BakeDataMap[Guid].KeyTimes.FindKey(KeyTime);
				if (Number == nullptr)
				{
					continue;
				}
				AActor* Actor = Cast<AActor>(RuntimeObject.Get());
				if (!Actor)
				{
					UActorComponent* ActorComponent = Cast<UActorComponent>(RuntimeObject.Get());
					if (ActorComponent)
					{
						Actor = ActorComponent->GetOwner();
					}
				}

				if (!Actor)
				{
					continue;
				}

				UCameraComponent* CameraComponent = MovieSceneHelpers::CameraComponentFromRuntimeObject(RuntimeObject.Get());

				// Cache transforms
				USceneComponent* Parent = nullptr;
				if (CameraComponent)
				{
					Parent = CameraComponent->GetAttachParent();
				} 
				else if (Actor->GetRootComponent())
				{
					Parent = Actor->GetRootComponent()->GetAttachParent();
				}
				
				// The CameraRig_rail updates the spline position tick, so it needs to be ticked manually while baking the frames
				while (Parent && Parent->GetOwner())
				{
					Parent->GetOwner()->Tick(0.03f);
					if (USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(Parent))
					{
						SkeletalMeshComponent->TickAnimation(0.f, false);

						SkeletalMeshComponent->RefreshBoneTransforms();
						SkeletalMeshComponent->RefreshFollowerComponents();
						SkeletalMeshComponent->UpdateComponentToWorld();
						SkeletalMeshComponent->FinalizeBoneTransform();
						SkeletalMeshComponent->MarkRenderTransformDirty();
						SkeletalMeshComponent->MarkRenderDynamicDataDirty();
					}
					Parent = Parent->GetAttachParent();
				}

				if (CameraComponent)
				{
					FTransform AdditiveOffset;
					float AdditiveFOVOffset;
					CameraComponent->GetAdditiveOffset(AdditiveOffset, AdditiveFOVOffset);

					FTransform Transform(Actor->GetActorRotation(), Actor->GetActorLocation());
					FTransform TransformWithAdditiveOffset = AdditiveOffset * Transform;
					FVector LocalTranslation = TransformWithAdditiveOffset.GetTranslation();
					FRotator LocalRotation = TransformWithAdditiveOffset.GetRotation().Rotator();

					BakeDataMap[Guid].Locations.Add(LocalTranslation);
					BakeDataMap[Guid].Rotations.Add(LocalRotation);
					BakeDataMap[Guid].Scales.Add(FVector::OneVector);
				}
				else
				{
					BakeDataMap[Guid].Locations.Add(Actor->GetActorLocation());
					BakeDataMap[Guid].Rotations.Add(Actor->GetActorRotation());
					BakeDataMap[Guid].Scales.Add(Actor->GetActorScale());
				}

			}
		}
	}

	const bool bDisableSectionsAfterBaking = Sequencer->GetSequencerSettings()->GetDisableSectionsAfterBaking();

	for (TPair<FGuid, FBakeData>& BakeData : BakeDataMap)
	{
		FGuid Guid = BakeData.Key;
		TArray<FFrameNumber> KeyTimes;
		BakeData.Value.KeyTimes.GenerateKeyArray(KeyTimes);
		// Disable or delete any constraint (attach/path) tracks
		AActor* AttachParentActor = nullptr;
		for (UMovieSceneTrack* Track : FocusedMovieScene->FindTracks(UMovieScene3DConstraintTrack::StaticClass(), Guid))
		{
			if (UMovieScene3DConstraintTrack* ConstraintTrack = Cast<UMovieScene3DConstraintTrack>(Track))
			{
				for (UMovieSceneSection* ConstraintSection : ConstraintTrack->GetAllSections())
				{
					FMovieSceneObjectBindingID ConstraintBindingID = (Cast<UMovieScene3DConstraintSection>(ConstraintSection))->GetConstraintBindingID();
					for (TWeakObjectPtr<> ParentObject : ConstraintBindingID.ResolveBoundObjects(Sequencer->GetFocusedTemplateID(), *Sequencer))
					{
						AttachParentActor = Cast<AActor>(ParentObject.Get());
						break;
					}
				}

				if (bDisableSectionsAfterBaking)
				{
					for (UMovieSceneSection* ConstraintSection : ConstraintTrack->GetAllSections())
					{
						ConstraintSection->Modify();
						ConstraintSection->SetIsActive(false);
					}
				}
				else
				{
					FocusedMovieScene->RemoveTrack(*ConstraintTrack);
				}
			}
		}

		// Disable or delete any transform tracks
		for (UMovieSceneTrack* Track : FocusedMovieScene->FindTracks(UMovieScene3DTransformTrack::StaticClass(), Guid))
		{
			if (UMovieScene3DTransformTrack* TransformTrack = Cast<UMovieScene3DTransformTrack>(Track))
			{
				if (bDisableSectionsAfterBaking)
				{
					for (UMovieSceneSection* TransformSection : TransformTrack->GetAllSections())
					{
						TransformSection->Modify();
						TransformSection->SetIsActive(false);
					}
				}
				else
				{
					FocusedMovieScene->RemoveTrack(*TransformTrack);
				}
			}
		}

		// Disable or delete any camera shake tracks
		for (UMovieSceneTrack* Track : FocusedMovieScene->FindTracks(UMovieSceneCameraShakeTrack::StaticClass(), Guid))
		{
			if (UMovieSceneCameraShakeTrack* CameraShakeTrack = Cast<UMovieSceneCameraShakeTrack>(Track))
			{
				if (bDisableSectionsAfterBaking)
				{
					for (UMovieSceneSection* CameraShakeSection : CameraShakeTrack->GetAllSections())
					{
						CameraShakeSection->Modify();
						CameraShakeSection->SetIsActive(false);
					}
				}
				else
				{
					FocusedMovieScene->RemoveTrack(*CameraShakeTrack);
				}
			}
		}

		// Reset position
		Sequencer->SetLocalTimeDirectly(ResetTime.Time);
		Sequencer->ForceEvaluate();

		FVector DefaultLocation = FVector::ZeroVector;
		FVector DefaultRotation = FVector::ZeroVector;
		FVector DefaultScale = FVector::OneVector;

		for (TWeakObjectPtr<> RuntimeObject : Sequencer->FindObjectsInCurrentSequence(Guid))
		{
			AActor* Actor = Cast<AActor>(RuntimeObject.Get());
			if (!Actor)
			{
				UActorComponent* ActorComponent = Cast<UActorComponent>(RuntimeObject.Get());
				if (ActorComponent)
				{
					Actor = ActorComponent->GetOwner();
				}
			}

			if (!Actor)
			{
				continue;
			}

			DefaultLocation = Actor->GetActorLocation();
			DefaultRotation = Actor->GetActorRotation().Euler();
			DefaultScale = Actor->GetActorScale();

			// Always detach from any existing parent
			Actor->DetachFromActor(FDetachmentTransformRules::KeepRelativeTransform);
		}
			
		// Create new transform track and section
		UMovieScene3DTransformTrack* TransformTrack = Cast<UMovieScene3DTransformTrack>(FocusedMovieScene->AddTrack(UMovieScene3DTransformTrack::StaticClass(), Guid));

		if (TransformTrack)
		{
			UMovieScene3DTransformSection* TransformSection = CastChecked<UMovieScene3DTransformSection>(TransformTrack->CreateNewSection());
			TransformTrack->AddSection(*TransformSection);

			TransformSection->SetRange(TRange<FFrameNumber>::All());

			TArrayView<FMovieSceneDoubleChannel*> DoubleChannels = TransformSection->GetChannelProxy().GetChannels<FMovieSceneDoubleChannel>();
			DoubleChannels[0]->SetDefault(DefaultLocation.X);
			DoubleChannels[1]->SetDefault(DefaultLocation.Y);
			DoubleChannels[2]->SetDefault(DefaultLocation.Z);
			DoubleChannels[3]->SetDefault(DefaultRotation.X);
			DoubleChannels[4]->SetDefault(DefaultRotation.Y);
			DoubleChannels[5]->SetDefault(DefaultRotation.Z);
			DoubleChannels[6]->SetDefault(DefaultScale.X);
			DoubleChannels[7]->SetDefault(DefaultScale.Y);
			DoubleChannels[8]->SetDefault(DefaultScale.Z);

			TArray<FVector> LocalTranslations, LocalRotations, LocalScales;
			LocalTranslations.SetNum(KeyTimes.Num());
			LocalRotations.SetNum(KeyTimes.Num());
			LocalScales.SetNum(KeyTimes.Num());

			for (int32 Counter = 0; Counter < KeyTimes.Num(); ++Counter)
			{
				FVector LocalTranslation = DefaultLocation;
				FVector LocalScale = DefaultScale;
				FRotator LocalRotation = DefaultRotation.Rotation();

				if (Counter < BakeData.Value.Locations.Num())
				{
					LocalTranslation = BakeData.Value.Locations[Counter];
				}
				if (Counter < BakeData.Value.Rotations.Num())
				{
					LocalRotation = BakeData.Value.Rotations[Counter];
				}
				if (Counter < BakeData.Value.Scales.Num())
				{
					LocalScale = BakeData.Value.Scales[Counter];
				}

				FTransform LocalTransform(LocalRotation, LocalTranslation, LocalScale);
				LocalTranslations[Counter] = LocalTransform.GetTranslation();
				LocalRotations[Counter] = LocalTransform.GetRotation().Euler();
				LocalScales[Counter] = LocalTransform.GetScale3D();
			}

			// Euler filter
			for (int32 Counter = 0; Counter < LocalRotations.Num() - 1; ++Counter)
			{
				FMath::WindRelativeAnglesDegrees(LocalRotations[Counter].X, LocalRotations[Counter + 1].X);
				FMath::WindRelativeAnglesDegrees(LocalRotations[Counter].Y, LocalRotations[Counter + 1].Y);
				FMath::WindRelativeAnglesDegrees(LocalRotations[Counter].Z, LocalRotations[Counter + 1].Z);							
			}
			if (SettingsInTick.BakingKeySettings == EBakingKeySettings::KeysOnly)
			{
				const EMovieSceneKeyInterpolation KeyInterpolation = Sequencer->GetSequencerSettings()->GetKeyInterpolation();

				for (int32 Counter = 0; Counter < KeyTimes.Num(); ++Counter)
				{
					int ChannelIndex = 0;
					FFrameNumber KeyTime = KeyTimes[Counter];
					{
						TMovieSceneChannelData<FMovieSceneDoubleValue> ChannelData = DoubleChannels[ChannelIndex++]->GetData();
						MovieSceneToolHelpers::SetOrAddKey(ChannelData, KeyTime, LocalTranslations[Counter].X);
					}
					{
						TMovieSceneChannelData<FMovieSceneDoubleValue> ChannelData = DoubleChannels[ChannelIndex++]->GetData();
						MovieSceneToolHelpers::SetOrAddKey(ChannelData, KeyTime, LocalTranslations[Counter].Y);
					}
					{
						TMovieSceneChannelData<FMovieSceneDoubleValue> ChannelData = DoubleChannels[ChannelIndex++]->GetData();
						MovieSceneToolHelpers::SetOrAddKey(ChannelData, KeyTime, LocalTranslations[Counter].Z);
					}
					{
						TMovieSceneChannelData<FMovieSceneDoubleValue> ChannelData = DoubleChannels[ChannelIndex++]->GetData();
						MovieSceneToolHelpers::SetOrAddKey(ChannelData, KeyTime, LocalRotations[Counter].X);
					}
					{
						TMovieSceneChannelData<FMovieSceneDoubleValue> ChannelData = DoubleChannels[ChannelIndex++]->GetData();
						MovieSceneToolHelpers::SetOrAddKey(ChannelData, KeyTime, LocalRotations[Counter].Y);

					}
					{
						TMovieSceneChannelData<FMovieSceneDoubleValue> ChannelData = DoubleChannels[ChannelIndex++]->GetData();
						MovieSceneToolHelpers::SetOrAddKey(ChannelData, KeyTime, LocalRotations[Counter].Z);
					}
					{
						TMovieSceneChannelData<FMovieSceneDoubleValue> ChannelData = DoubleChannels[ChannelIndex++]->GetData();
						MovieSceneToolHelpers::SetOrAddKey(ChannelData, KeyTime, LocalScales[Counter].X);
					}
					{
						TMovieSceneChannelData<FMovieSceneDoubleValue> ChannelData = DoubleChannels[ChannelIndex++]->GetData();
						MovieSceneToolHelpers::SetOrAddKey(ChannelData, KeyTime, LocalScales[Counter].Y);
					}
					{
						TMovieSceneChannelData<FMovieSceneDoubleValue> ChannelData = DoubleChannels[ChannelIndex++]->GetData();
						MovieSceneToolHelpers::SetOrAddKey(ChannelData, KeyTime, LocalScales[Counter].Z);
					}
				}
			}
			else
			{
				for (int32 Counter = 0; Counter < KeyTimes.Num(); ++Counter)
				{
					FFrameNumber KeyTime = KeyTimes[Counter];
					DoubleChannels[0]->AddLinearKey(KeyTime, LocalTranslations[Counter].X);
					DoubleChannels[1]->AddLinearKey(KeyTime, LocalTranslations[Counter].Y);
					DoubleChannels[2]->AddLinearKey(KeyTime, LocalTranslations[Counter].Z);
					DoubleChannels[3]->AddLinearKey(KeyTime, LocalRotations[Counter].X);
					DoubleChannels[4]->AddLinearKey(KeyTime, LocalRotations[Counter].Y);
					DoubleChannels[5]->AddLinearKey(KeyTime, LocalRotations[Counter].Z);
					DoubleChannels[6]->AddLinearKey(KeyTime, LocalScales[Counter].X);
					DoubleChannels[7]->AddLinearKey(KeyTime, LocalScales[Counter].Y);
					DoubleChannels[8]->AddLinearKey(KeyTime, LocalScales[Counter].Z);
				}
				if (SettingsInTick.bReduceKeys == true)
				{
					FKeyDataOptimizationParams Param;
					Param.bAutoSetInterpolation = true;
					Param.Tolerance = SettingsInTick.Tolerance;
					TRange<FFrameNumber> Range(SettingsInTick.StartFrame, SettingsInTick.EndFrame);
					Param.Range = Range;
					MovieSceneToolHelpers::OptimizeSection(Param, TransformSection);
				}
			}
		}
	}
	
	Sequencer->NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::MovieSceneStructureItemsChanged );
	return true;
}

void ULevelSequenceEditorSubsystem::FixActorReferences()
{
	TSharedPtr<ISequencer> Sequencer = GetActiveSequencer();
	if (Sequencer == nullptr)
	{
		return;
	}

	UWorld* PlaybackContext = Sequencer->GetPlaybackContext()->GetWorld();
	if (!PlaybackContext)
	{
		return;
	}

	UMovieScene* FocusedMovieScene = Sequencer->GetFocusedMovieSceneSequence()->GetMovieScene();
	if (!FocusedMovieScene)
	{
		return;
	}

	if (FocusedMovieScene->IsReadOnly())
	{
		FSequencerUtilities::ShowReadOnlyError();
		return;
	}

	FScopedTransaction FixActorReferencesTransaction(LOCTEXT("FixActorReferences", "Fix Actor References"));

	TMap<FString, AActor*> ActorNameToActorMap;

	for (TActorIterator<AActor> ActorItr(PlaybackContext); ActorItr; ++ActorItr)
	{
		// Same as with the Object Iterator, access the subclass instance with the * or -> operators.
		AActor* Actor = *ActorItr;
		ActorNameToActorMap.Add(Actor->GetActorLabel(), Actor);
	}

	// Cache the possessables to fix up first since the bindings will change as the fix ups happen.
	TArray<FMovieScenePossessable> ActorsPossessablesToFix;
	for (int32 i = 0; i < FocusedMovieScene->GetPossessableCount(); i++)
	{
		FMovieScenePossessable& Possessable = FocusedMovieScene->GetPossessable(i);
		// Possessables with parents are components so ignore them.
		if (Possessable.GetParent().IsValid() == false)
		{
			if (Sequencer->FindBoundObjects(Possessable.GetGuid(), Sequencer->GetFocusedTemplateID()).Num() == 0)
			{
				ActorsPossessablesToFix.Add(Possessable);
			}
		}
	}

	// For the possessables to fix, look up the actors by name and reassign them if found.
	TMap<FGuid, FGuid> OldGuidToNewGuidMap;
	for (const FMovieScenePossessable& ActorPossessableToFix : ActorsPossessablesToFix)
	{
		AActor* ActorPtr = ActorNameToActorMap.FindRef(ActorPossessableToFix.GetName());
		if (ActorPtr != nullptr)
		{
			FGuid OldGuid = ActorPossessableToFix.GetGuid();

			// The actor might have an existing guid while the possessable with the same name might not. 
			// In that case, make sure we also replace the existing guid with the new guid 
			FGuid ExistingGuid = Sequencer->FindObjectId(*ActorPtr, Sequencer->GetFocusedTemplateID());

			FGuid NewGuid = FSequencerUtilities::AssignActor(Sequencer.ToSharedRef(), ActorPtr, ActorPossessableToFix.GetGuid());

			OldGuidToNewGuidMap.Add(OldGuid, NewGuid);

			if (ExistingGuid.IsValid())
			{
				OldGuidToNewGuidMap.Add(ExistingGuid, NewGuid);
			}
		}
	}

	for (TPair<FGuid, FGuid> GuidPair : OldGuidToNewGuidMap)
	{
		FSequencerUtilities::UpdateBindingIDs(Sequencer.ToSharedRef(), GuidPair.Key, GuidPair.Value);
	}

	Sequencer->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemsChanged);
}

void ULevelSequenceEditorSubsystem::AddActorsToBindingInternal()
{
	TSharedPtr<ISequencer> Sequencer = GetActiveSequencer();
	if (Sequencer == nullptr)
	{
		return;
	}

	TArray<FGuid> ObjectBindings;
	Sequencer->GetSelectedObjects(ObjectBindings);
	if (ObjectBindings.Num() == 0)
	{
		return;
	}

	FMovieSceneBindingProxy BindingProxy(ObjectBindings[0], Sequencer->GetFocusedMovieSceneSequence());

	TArray<AActor*> SelectedActors;
	GEditor->GetSelectedActors()->GetSelectedObjects<AActor>(SelectedActors); 
	AddActorsToBinding(SelectedActors, BindingProxy);
}

void ULevelSequenceEditorSubsystem::AddActorsToBinding(const TArray<AActor*>& Actors, const FMovieSceneBindingProxy& ObjectBinding)
{
	TSharedPtr<ISequencer> Sequencer = GetActiveSequencer();
	if (Sequencer == nullptr)
	{
		return;
	}

	FSequencerUtilities::AddActorsToBinding(Sequencer.ToSharedRef(), Actors, ObjectBinding);
}

void ULevelSequenceEditorSubsystem::ReplaceBindingWithActorsInternal()
{
	TSharedPtr<ISequencer> Sequencer = GetActiveSequencer();
	if (Sequencer == nullptr)
	{
		return;
	}

	TArray<FGuid> ObjectBindings;
	Sequencer->GetSelectedObjects(ObjectBindings);
	if (ObjectBindings.Num() == 0)
	{
		return;
	}

	FMovieSceneBindingProxy BindingProxy(ObjectBindings[0], Sequencer->GetFocusedMovieSceneSequence());

	TArray<AActor*> SelectedActors;
	GEditor->GetSelectedActors()->GetSelectedObjects<AActor>(SelectedActors); 
	ReplaceBindingWithActors(SelectedActors, BindingProxy);
}

void ULevelSequenceEditorSubsystem::ReplaceBindingWithActors(const TArray<AActor*>& Actors, const FMovieSceneBindingProxy& ObjectBinding)
{
	TSharedPtr<ISequencer> Sequencer = GetActiveSequencer();
	if (Sequencer == nullptr)
	{
		return;
	}

	FSequencerUtilities::ReplaceBindingWithActors(Sequencer.ToSharedRef(), Actors, ObjectBinding);
}

void ULevelSequenceEditorSubsystem::RemoveActorsFromBindingInternal()
{
	TSharedPtr<ISequencer> Sequencer = GetActiveSequencer();
	if (Sequencer == nullptr)
	{
		return;
	}

	TArray<FGuid> ObjectBindings;
	Sequencer->GetSelectedObjects(ObjectBindings);
	if (ObjectBindings.Num() == 0)
	{
		return;
	}

	FMovieSceneBindingProxy BindingProxy(ObjectBindings[0], Sequencer->GetFocusedMovieSceneSequence());

	TArray<AActor*> SelectedActors;
	GEditor->GetSelectedActors()->GetSelectedObjects<AActor>(SelectedActors); 
	RemoveActorsFromBinding(SelectedActors, BindingProxy);
}

void ULevelSequenceEditorSubsystem::RemoveActorsFromBinding(const TArray<AActor*>& Actors, const FMovieSceneBindingProxy& ObjectBinding)
{
	TSharedPtr<ISequencer> Sequencer = GetActiveSequencer();
	if (Sequencer == nullptr)
	{
		return;
	}

	FSequencerUtilities::RemoveActorsFromBinding(Sequencer.ToSharedRef(), Actors, ObjectBinding);
}

void ULevelSequenceEditorSubsystem::RemoveAllBindingsInternal()
{
	TSharedPtr<ISequencer> Sequencer = GetActiveSequencer();
	if (Sequencer == nullptr)
	{
		return;
	}

	TArray<FGuid> ObjectBindings;
	Sequencer->GetSelectedObjects(ObjectBindings);
	if (ObjectBindings.Num() == 0)
	{
		return;
	}

	FMovieSceneBindingProxy BindingProxy(ObjectBindings[0], Sequencer->GetFocusedMovieSceneSequence());

	RemoveAllBindings(BindingProxy);
}

void ULevelSequenceEditorSubsystem::RemoveAllBindings(const FMovieSceneBindingProxy& ObjectBinding)
{
	TSharedPtr<ISequencer> Sequencer = GetActiveSequencer();
	if (Sequencer == nullptr)
	{
		return;
	}

	UMovieSceneSequence* Sequence = Sequencer->GetFocusedMovieSceneSequence();
	if (!Sequence)
	{
		return;
	}

	UMovieScene* MovieScene = Sequence->GetMovieScene();
	if (!MovieScene)
	{
		return;
	}

	FScopedTransaction RemoveAllBindings(LOCTEXT("RemoveAllBindings", "Remove All Bound Objects"));

	Sequence->Modify();
	MovieScene->Modify();

	// Unbind objects
	FGuid Guid = ObjectBinding.BindingID;
	Sequence->UnbindPossessableObjects(Guid);

	Sequencer->RestorePreAnimatedState();

	Sequencer->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemsChanged);
}

void ULevelSequenceEditorSubsystem::RemoveInvalidBindingsInternal()
{
	TSharedPtr<ISequencer> Sequencer = GetActiveSequencer();
	if (Sequencer == nullptr)
	{
		return;
	}

	TArray<FGuid> ObjectBindings;
	Sequencer->GetSelectedObjects(ObjectBindings);
	if (ObjectBindings.Num() == 0)
	{
		return;
	}

	FMovieSceneBindingProxy BindingProxy(ObjectBindings[0], Sequencer->GetFocusedMovieSceneSequence());

	RemoveInvalidBindings(BindingProxy);
}

void ULevelSequenceEditorSubsystem::RemoveInvalidBindings(const FMovieSceneBindingProxy& ObjectBinding)
{
	TSharedPtr<ISequencer> Sequencer = GetActiveSequencer();
	if (Sequencer == nullptr)
	{
		return;
	}

	UMovieSceneSequence* Sequence = Sequencer->GetFocusedMovieSceneSequence();
	if (!Sequence)
	{
		return;
	}

	UMovieScene* MovieScene = Sequence->GetMovieScene();
	if (!MovieScene)
	{
		return;
	}
	
	FScopedTransaction RemoveInvalidBindings(LOCTEXT("RemoveMissing", "Remove Missing Objects"));

	Sequence->Modify();
	MovieScene->Modify();

	// Unbind objects
	FGuid Guid = ObjectBinding.BindingID;
	Sequence->UnbindInvalidObjects(Guid, Sequencer->GetPlaybackContext());

	// Update label
	UClass* ActorClass = nullptr;

	TArray<AActor*> ValidActors;
	for (TWeakObjectPtr<> Ptr : Sequencer->FindObjectsInCurrentSequence(Guid))
	{
		if (AActor* Actor = Cast<AActor>(Ptr.Get()))
		{
			ActorClass = Actor->GetClass();
			ValidActors.Add(Actor);
		}
	}

	FMovieScenePossessable* Possessable = MovieScene->FindPossessable(Guid);
	if (Possessable && ActorClass != nullptr && ValidActors.Num() != 0)
	{
		if (ValidActors.Num() > 1)
		{
			FString NewLabel = ActorClass->GetName() + FString::Printf(TEXT(" (%d)"), ValidActors.Num());

			Possessable->SetName(NewLabel);
		}
		else
		{
			Possessable->SetName(ValidActors[0]->GetActorLabel());
		}
	}

	Sequencer->RestorePreAnimatedState();

	Sequencer->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemsChanged);
}

void ULevelSequenceEditorSubsystem::AddAssignActorMenu(FMenuBuilder& MenuBuilder)
{
	TSharedPtr<ISequencer> Sequencer = GetActiveSequencer();
	if (Sequencer == nullptr)
	{
		return;
	}

	TArray<FGuid> ObjectBindings;
	Sequencer->GetSelectedObjects(ObjectBindings);
	if (ObjectBindings.Num() == 0)
	{
		return;
	}

	MenuBuilder.AddMenuEntry(FLevelSequenceEditorCommands::Get().AddActorsToBinding);
	MenuBuilder.AddMenuEntry(FLevelSequenceEditorCommands::Get().ReplaceBindingWithActors);
	MenuBuilder.AddMenuEntry(FLevelSequenceEditorCommands::Get().RemoveActorsFromBinding);
	MenuBuilder.AddMenuEntry(FLevelSequenceEditorCommands::Get().RemoveAllBindings);
	MenuBuilder.AddMenuEntry(FLevelSequenceEditorCommands::Get().RemoveInvalidBindings);

	FMovieSceneBindingProxy BindingProxy(ObjectBindings[0], Sequencer->GetFocusedMovieSceneSequence());

	TSet<const AActor*> BoundObjects;
	{
		for (TWeakObjectPtr<> Ptr : Sequencer->FindObjectsInCurrentSequence(ObjectBindings[0]))
		{
			if (const AActor* Actor = Cast<AActor>(Ptr.Get()))
			{
				BoundObjects.Add(Actor);
			}
		}
	}

	auto IsActorValidForAssignment = [BoundObjects](const AActor* InActor){
		return !BoundObjects.Contains(InActor);
	};

	// Set up a menu entry to assign an actor to the object binding node
	FSceneOutlinerInitializationOptions InitOptions;
	{
		// We hide the header row to keep the UI compact.
		InitOptions.bShowHeaderRow = false;
		InitOptions.bShowSearchBox = true;
		InitOptions.bShowCreateNewFolder = false;
		InitOptions.bFocusSearchBoxWhenOpened = true;
		// Only want the actor label column
		InitOptions.ColumnMap.Add(FSceneOutlinerBuiltInColumnTypes::Label(), FSceneOutlinerColumnInfo(ESceneOutlinerColumnVisibility::Visible, 0));
		
		// Only display actors that are not possessed already
		InitOptions.Filters->AddFilterPredicate<FActorTreeItem>(FActorTreeItem::FFilterPredicate::CreateLambda( IsActorValidForAssignment ) );
	}

	// actor selector to allow the user to choose an actor
	FSceneOutlinerModule& SceneOutlinerModule = FModuleManager::LoadModuleChecked<FSceneOutlinerModule>("SceneOutliner");
	TSharedRef< SWidget > MiniSceneOutliner =
		SNew( SBox )
		.MaxDesiredHeight(400.0f)
		.WidthOverride(300.0f)
		[
			SceneOutlinerModule.CreateActorPicker(
				InitOptions,
				FOnActorPicked::CreateLambda([=](AActor* Actor){
					// Create a new binding for this actor
					FSlateApplication::Get().DismissAllMenus();
					FSequencerUtilities::AssignActor(Sequencer.ToSharedRef(), Actor, ObjectBindings[0]);
				})
			)
		];

	MenuBuilder.AddMenuSeparator();
	MenuBuilder.AddWidget(MiniSceneOutliner, FText::GetEmpty(), true);
}

void ULevelSequenceEditorSubsystem::AddBindingPropertiesMenu(FMenuBuilder& MenuBuilder)
{
	TSharedPtr<ISequencer> Sequencer = GetActiveSequencer();
	if (Sequencer == nullptr)
	{
		return;
	}

	TArray<FGuid> ObjectBindings;
	Sequencer->GetSelectedObjects(ObjectBindings);
	if (ObjectBindings.Num() == 0)
	{
		return;
	}

	UMovieSceneSequence* Sequence = Sequencer->GetFocusedMovieSceneSequence();
	if (!Sequence)
	{
		return;
	}
	if (FMovieSceneBindingReferences* BindingReferences = Sequence->GetBindingReferences())
	{
		TSharedRef<FStructOnScope> LocatorsStruct = MakeShareable(new FStructOnScope(FMovieSceneUniversalLocatorList::StaticStruct()));
		FMovieSceneUniversalLocatorList* Locators = (FMovieSceneUniversalLocatorList*)LocatorsStruct->GetStructMemory();
		Algo::Transform(BindingReferences->GetReferences(ObjectBindings[0]), Locators->Bindings, [](const FMovieSceneBindingReference& Reference) 
			{ 
				return FMovieSceneUniversalLocatorInfo{ Reference.Locator, Reference.ResolveFlags };
			});

		MenuBuilder.AddMenuSeparator();

		NotifyHook = FBindingPropertiesNotifyHook(Sequence);
		// Set up a details panel for the list of locators
		FDetailsViewArgs DetailsViewArgs;
		{
			DetailsViewArgs.bAllowSearch = false;
			DetailsViewArgs.bCustomFilterAreaLocation = true;
			DetailsViewArgs.bCustomNameAreaLocation = true;
			DetailsViewArgs.bHideSelectionTip = true;
			DetailsViewArgs.bLockable = false;
			DetailsViewArgs.bSearchInitialKeyFocus = true;
			DetailsViewArgs.bUpdatesFromSelection = false;
			DetailsViewArgs.bShowOptions = false;
			DetailsViewArgs.bShowModifiedPropertiesOption = false;
			DetailsViewArgs.bShowScrollBar = false;
			DetailsViewArgs.bAllowMultipleTopLevelObjects = false;
			DetailsViewArgs.NotifyHook = &NotifyHook;
		}

		FStructureDetailsViewArgs StructureViewArgs;
		{
			StructureViewArgs.bShowObjects = true;
			StructureViewArgs.bShowAssets = true;
			StructureViewArgs.bShowClasses = true;
			StructureViewArgs.bShowInterfaces = true;
		}

		TSharedRef<IStructureDetailsView> StructureDetailsView = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor")
			.CreateStructureDetailView(DetailsViewArgs, StructureViewArgs, nullptr);

		StructureDetailsView->SetStructureData(LocatorsStruct);
		StructureDetailsView->GetOnFinishedChangingPropertiesDelegate().AddUObject(this, &ULevelSequenceEditorSubsystem::OnFinishedChangingLocators, StructureDetailsView,  LocatorsStruct, ObjectBindings[0]);

		MenuBuilder.AddWidget(StructureDetailsView->GetWidget().ToSharedRef(), FText::GetEmpty(), true);
	}
}



void ULevelSequenceEditorSubsystem::FBindingPropertiesNotifyHook::NotifyPreChange(FProperty* PropertyAboutToChange)
{
	if (PropertyAboutToChange != nullptr)
	{
		GEditor->BeginTransaction(FText::Format(LOCTEXT("EditProperty", "Edit {0}"), PropertyAboutToChange->GetDisplayNameText()));

		ObjectToModify->Modify();
	}
}

void ULevelSequenceEditorSubsystem::FBindingPropertiesNotifyHook::NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent, FProperty* PropertyThatChanged)
{
	GEditor->EndTransaction();
}


void ULevelSequenceEditorSubsystem::OnFinishedChangingLocators(const FPropertyChangedEvent& PropertyChangedEvent, TSharedRef<IStructureDetailsView> StructDetailsView, TSharedRef<FStructOnScope> LocatorsStruct, FGuid ObjectBindingID)
{
	const FScopedTransaction Transaction(LOCTEXT("ChangeBindingProperties", "Change Binding Properties"));

	auto Locators = (FMovieSceneUniversalLocatorList*)LocatorsStruct->GetStructMemory();
	TSharedPtr<ISequencer> Sequencer = GetActiveSequencer();
	if (Sequencer == nullptr)
	{
		return;
	}
	UMovieSceneSequence* Sequence = Sequencer->GetFocusedMovieSceneSequence();
	if (!Sequence)
	{
		return;
	}

	UMovieScene* MovieScene = Sequence->GetMovieScene();
	if (!MovieScene)
	{
		return;
	}
	if (FMovieSceneBindingReferences* BindingReferences = Sequence->GetBindingReferences())
	{
		MovieScene->Modify();
		Sequence->Modify();
		// Clear the previous binding
		if (FMovieSceneObjectCache* Cache = Sequencer->State.FindObjectCache(Sequencer->GetFocusedTemplateID()))
		{
			Cache->UnloadBinding(ObjectBindingID, Sequencer->GetSharedPlaybackState());
		}
		BindingReferences->RemoveBinding(ObjectBindingID);

		// Add the new updated bindings
		for (FMovieSceneUniversalLocatorInfo& LocatorInfo : Locators->Bindings)
		{
			if (PropertyChangedEvent.Property != nullptr && PropertyChangedEvent.Property->GetFName() == TEXT("Locator"))
			{
				// Ensure flags are initialized from scratch
				const FMovieSceneBindingReference* NewRef = BindingReferences->AddBinding(ObjectBindingID, MoveTemp(LocatorInfo.Locator));
				if (NewRef)
				{
					LocatorInfo.ResolveFlags = NewRef->ResolveFlags;
				}
			}
			else
			{
				BindingReferences->AddBinding(ObjectBindingID, MoveTemp(LocatorInfo.Locator), LocatorInfo.ResolveFlags);
			}
		}

		Sequencer->State.Invalidate(ObjectBindingID, Sequencer->GetFocusedTemplateID());

		// Update the object class and DisplayName
		TArrayView<TWeakObjectPtr<>> ObjectsInCurrentSequence = Sequencer->FindObjectsInCurrentSequence(ObjectBindingID);
		UClass* ObjectClass = nullptr;

		for (TWeakObjectPtr<> Ptr : ObjectsInCurrentSequence)
		{
			if (UObject* BoundObject = Ptr.Get())
			{
				if (ObjectClass == nullptr)
				{
					ObjectClass = BoundObject->GetClass();
				}
				else
				{
					ObjectClass = UClass::FindCommonBase(BoundObject->GetClass(), ObjectClass);
				}
			}
		}

		// Update label
		if (ObjectsInCurrentSequence.Num() > 0)
		{
			FMovieScenePossessable* Possessable = MovieScene->FindPossessable(ObjectBindingID);
			if (Possessable && ObjectClass != nullptr)
			{
				if (ObjectsInCurrentSequence.Num() > 1)
				{
					FString NewLabel = ObjectClass->GetName() + FString::Printf(TEXT(" (%d)"), ObjectsInCurrentSequence.Num());
					Possessable->SetName(NewLabel);
				}
				else if (AActor* Actor = Cast<AActor>(ObjectsInCurrentSequence[0].Get()))
				{
					Possessable->SetName(Actor->GetActorLabel());
				}
				else
				{
					Possessable->SetName(ObjectClass->GetName());
				}

				Possessable->SetPossessedObjectClass(ObjectClass);
			}
		}

		Sequencer->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemsChanged);

		// Force evaluate the Sequencer after clearing the cache (which the above will do) so that any newly loaded actors will be loaded as part of the transaction
		Sequencer->ForceEvaluate();

		// Send the OnAddBinding message, which will add a Binding Lifetime Track if necessary
		Sequencer->OnAddBinding(ObjectBindingID, MovieScene);

		// Re-copy the locator info back into the struct details
		Locators->Bindings.Empty();
		Algo::Transform(BindingReferences->GetReferences(ObjectBindingID), Locators->Bindings, [](const FMovieSceneBindingReference& Reference)
			{
				return FMovieSceneUniversalLocatorInfo{ Reference.Locator, Reference.ResolveFlags };
			});


		// Force the struct details view to refresh
		StructDetailsView->GetDetailsView()->InvalidateCachedState();
	}
}

void ULevelSequenceEditorSubsystem::GetRebindComponentNames(TArray<FName>& OutComponentNames)
{
	OutComponentNames.Empty();

	TSharedPtr<ISequencer> Sequencer = GetActiveSequencer();
	if (Sequencer == nullptr)
	{
		return;
	}

	UMovieSceneSequence* Sequence = Sequencer->GetFocusedMovieSceneSequence();
	if (!Sequence)
	{
		return;
	}

	UMovieScene* MovieScene = Sequence->GetMovieScene();
	if (!MovieScene)
	{
		return;
	}

	TArray<FGuid> ObjectBindings;
	Sequencer->GetSelectedObjects(ObjectBindings);
	if (ObjectBindings.Num() == 0)
	{
		return;
	}

	FGuid ComponentGuid = ObjectBindings[0];

	FMovieScenePossessable* ComponentPossessable = MovieScene->FindPossessable(ComponentGuid);

	FGuid ActorParentGuid = ComponentPossessable ? ComponentPossessable->GetParent() : FGuid();

	TArrayView<TWeakObjectPtr<>> ObjectsInCurrentSequence = Sequencer->FindObjectsInCurrentSequence(ActorParentGuid);

	const AActor* Actor = nullptr;
	for (TWeakObjectPtr<> Ptr : ObjectsInCurrentSequence)
	{
		Actor = Cast<AActor>(Ptr.Get());
		if (Actor)
		{
			break;
		}
	}

	if (!Actor)
	{
		return;
	}
		
	FClassViewerModule& ClassViewerModule = FModuleManager::LoadModuleChecked<FClassViewerModule>("ClassViewer");
	const TSharedPtr<IClassViewerFilter>& GlobalClassFilter = ClassViewerModule.GetGlobalClassViewerFilter();
	TSharedRef<FClassViewerFilterFuncs> ClassFilterFuncs = ClassViewerModule.CreateFilterFuncs();
	FClassViewerInitializationOptions ClassViewerOptions = {};

	for (UActorComponent* Component : Actor->GetComponents())
	{
		if (Component && ComponentPossessable && Component->GetName() != ComponentPossessable->GetName())
		{
			bool bValidComponent = !Component->IsVisualizationComponent();

			if (GlobalClassFilter.IsValid())
			{
				// Hack - forcibly allow USkeletalMeshComponentBudgeted until FORT-527888
				static const FName SkeletalMeshComponentBudgetedClassName(TEXT("SkeletalMeshComponentBudgeted"));
				if (Component->GetClass()->GetName() == SkeletalMeshComponentBudgetedClassName)
				{
					bValidComponent = true;
				}
				else
				{
					bValidComponent = GlobalClassFilter->IsClassAllowed(ClassViewerOptions, Component->GetClass(), ClassFilterFuncs);
				}
			}

			if (bValidComponent)
			{
				OutComponentNames.Add(Component->GetFName());
			}
		}
	}
	OutComponentNames.Sort(FNameFastLess());
}

void ULevelSequenceEditorSubsystem::RebindComponentMenu(FMenuBuilder& MenuBuilder)
{
	TArray<FName> ComponentNames;
	GetRebindComponentNames(ComponentNames);

	for (const FName& ComponentName : ComponentNames)
	{
		FText RebindComponentLabel = FText::FromName(ComponentName);
		MenuBuilder.AddMenuEntry(
			RebindComponentLabel, 
			FText(), 
			FSlateIcon(), 
			FUIAction(FExecuteAction::CreateLambda([this, ComponentName]() { RebindComponentInternal(ComponentName); } ) ) );
	}
}

void ULevelSequenceEditorSubsystem::RebindComponentInternal(const FName& ComponentName)
{
	TSharedPtr<ISequencer> Sequencer = GetActiveSequencer();
	if (Sequencer == nullptr)
	{
		return;
	}

	TArray<FGuid> ObjectBindings;
	Sequencer->GetSelectedObjects(ObjectBindings);
	if (ObjectBindings.Num() == 0)
	{
		return;
	}

	TArray<FMovieSceneBindingProxy> BindingProxies;
	for (const FGuid& ObjectBinding : ObjectBindings)
	{
		FMovieSceneBindingProxy BindingProxy(ObjectBinding, Sequencer->GetFocusedMovieSceneSequence());
		BindingProxies.Add(BindingProxy);
	}

	RebindComponent(BindingProxies, ComponentName);
}

void ULevelSequenceEditorSubsystem::RebindComponent(const TArray<FMovieSceneBindingProxy>& PossessableBindings, const FName& ComponentName)
{
	TSharedPtr<ISequencer> Sequencer = GetActiveSequencer();
	if (Sequencer == nullptr)
	{
		return;
	}

	UMovieSceneSequence* Sequence = Sequencer->GetFocusedMovieSceneSequence();
	if (!Sequence)
	{
		return;
	}

	UMovieScene* MovieScene = Sequence->GetMovieScene();
	if (!MovieScene)
	{
		return;
	}

	FScopedTransaction RebindComponent(LOCTEXT("RebindComponent", "Rebind Component"));

	Sequence->Modify();
	MovieScene->Modify();

	bool bAnythingChanged = false;
	for (const FMovieSceneBindingProxy& PossessableBinding : PossessableBindings)
	{
		FMovieScenePossessable* ComponentPossessable = MovieScene->FindPossessable(PossessableBinding.BindingID);

		FGuid ActorParentGuid = ComponentPossessable ? ComponentPossessable->GetParent() : FGuid();

		TArrayView<TWeakObjectPtr<>> ObjectsInCurrentSequence = Sequencer->FindObjectsInCurrentSequence(ActorParentGuid);

		for (TWeakObjectPtr<> Ptr : ObjectsInCurrentSequence)
		{
			if (const AActor* Actor = Cast<AActor>(Ptr.Get()))
			{
				for (UActorComponent* Component : Actor->GetComponents())
				{
					if (Component->GetFName() == ComponentName)
					{
						FGuid ComponentBinding = Sequence->CreatePossessable(Component);
						
						if (PossessableBinding.BindingID.IsValid() && ComponentBinding.IsValid())
						{
							MovieScene->MoveBindingContents(PossessableBinding.BindingID, ComponentBinding);

							bAnythingChanged = true;
						}
					}
				}
			}
		}
	}

	if (bAnythingChanged)
	{
		Sequencer->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemsChanged);
	}
}

#undef LOCTEXT_NAMESPACE