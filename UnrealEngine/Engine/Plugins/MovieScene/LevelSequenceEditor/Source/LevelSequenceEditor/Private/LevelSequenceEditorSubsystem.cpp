// Copyright Epic Games, Inc. All Rights Reserved.

#include "LevelSequenceEditorSubsystem.h"

#include "Compilation/MovieSceneCompiledDataManager.h"
#include "ISequencer.h"
#include "ISequencerModule.h"
#include "LevelSequence.h"
#include "LevelSequenceEditorCommands.h"
#include "MovieScene.h"
#include "MovieSceneCommonHelpers.h"
#include "MovieSceneSection.h"
#include "MovieSceneSequence.h"
#include "MovieSceneTimeHelpers.h"
#include "SequencerSettings.h"
#include "SequencerUtilities.h"
#include "MovieSceneBindingProxy.h"
#include "SequenceTimeUnit.h"
#include "Sections/MovieScene3DConstraintSection.h"
#include "Sections/MovieScene3DTransformSection.h"
#include "Tracks/MovieScene3DTransformTrack.h"
#include "Tracks/MovieScene3DConstraintTrack.h"
#include "Tracks/MovieSceneCameraShakeTrack.h"

#include "ActorTreeItem.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EngineUtils.h"
#include "Engine/Selection.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Notifications/NotificationManager.h"
#include "GameFramework/Actor.h"
#include "HAL/PlatformApplicationMisc.h"
#include "IContentBrowserSingleton.h"
#include "LevelEditor.h"
#include "Modules/ModuleManager.h"
#include "SceneOutlinerModule.h"
#include "SceneOutlinerPublicTypes.h"
#include "ScopedTransaction.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"

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
	TArray<FMovieSceneBindingProxy> OutBindings;
	if (!FSequencerUtilities::PasteBindings(TextToImport, Sequencer.ToSharedRef(), PasteBindingsParams, OutBindings, PasteErrors))
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

		const FFrameNumber SectionSourceStartFrameNumber = SectionSourceTimecode.ToFrameNumber(TickResolution);

		// Account for any trimming at the start of the section when computing the
		// target frame number to move this section to.
		const FFrameNumber SectionOffsetFrames = Section->GetOffsetTime().Get(FFrameTime()).FloorToFrame();
		const FFrameNumber TargetFrameNumber = SectionSourceStartFrameNumber + SectionOffsetFrames;

		const FFrameNumber SectionCurrentStartFrameNumber = Section->GetInclusiveStartFrame();

		const FFrameNumber Delta = -(SectionCurrentStartFrameNumber - TargetFrameNumber);

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

	const UMovieSceneSection* FirstSection = SectionsToSync[0];
	const FFrameNumber FirstSectionSourceTimecode = FirstSection->TimecodeSource.Timecode.ToFrameNumber(TickResolution);
	const FFrameNumber FirstSectionCurrentStartFrame = FirstSection->GetInclusiveStartFrame();
	const FFrameNumber FirstSectionOffsetFrames = FirstSection->GetOffsetTime().Get(FFrameTime()).FloorToFrame();
	SectionsToSync.RemoveAt(0);

	for (UMovieSceneSection* Section : SectionsToSync)
	{
		const FFrameNumber SectionSourceTimecode = Section->TimecodeSource.Timecode.ToFrameNumber(TickResolution);
		const FFrameNumber SectionCurrentStartFrame = Section->GetInclusiveStartFrame();
		const FFrameNumber SectionOffsetFrames = Section->GetOffsetTime().Get(FFrameTime()).FloorToFrame();

		const FFrameNumber TimecodeDelta = SectionSourceTimecode - FirstSectionSourceTimecode;
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

	FFrameNumber InFrame = UE::MovieScene::DiscreteInclusiveLower(FocusedMovieScene->GetPlaybackRange());
	FFrameNumber OutFrame = UE::MovieScene::DiscreteExclusiveUpper(FocusedMovieScene->GetPlaybackRange());

	TArray<FGuid> ObjectBindings;
	Sequencer->GetSelectedObjects(ObjectBindings);

	TArray<FMovieSceneBindingProxy> BindingProxies;
	for (FGuid Guid : ObjectBindings)
	{
		BindingProxies.Add(FMovieSceneBindingProxy(Guid, Sequencer->GetFocusedMovieSceneSequence()));
	}

	FMovieSceneScriptingParams Params;
	Params.TimeUnit = ESequenceTimeUnit::TickResolution;
	BakeTransform(BindingProxies, FFrameTime(InFrame), FFrameTime(OutFrame), ConvertFrameTime(1, DisplayRate, TickResolution), Params);
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

	FScopedTransaction BakeTransform(LOCTEXT("BakeTransform", "Bake Transform"));

	FocusedMovieScene->Modify();

	FQualifiedFrameTime ResetTime = Sequencer->GetLocalTime();

	FFrameRate TickResolution = FocusedMovieScene->GetTickResolution();
	FFrameRate DisplayRate = FocusedMovieScene->GetDisplayRate();

	FFrameTime InFrame = BakeInTime;
	FFrameTime OutFrame = BakeOutTime;
	FFrameTime Interval = BakeInterval;

	if (Params.TimeUnit == ESequenceTimeUnit::DisplayRate)
	{
		InFrame = ConvertFrameTime(BakeInTime, DisplayRate, TickResolution);
		OutFrame = ConvertFrameTime(BakeOutTime, DisplayRate, TickResolution);
		Interval = ConvertFrameTime(BakeInterval, DisplayRate, TickResolution);
	}

	struct FBakeData
	{
		TArray<FVector> Locations;
		TArray<FRotator> Rotations;
		TArray<FVector> Scales;
		TArray<FFrameNumber> KeyTimes;
	};

	TMap<FGuid, FBakeData> BakeDataMap;
	for (const FMovieSceneBindingProxy& ObjectBinding : ObjectBindings)
	{
		BakeDataMap.Add(ObjectBinding.BindingID);
	}

	FMovieSceneSequenceTransform RootToLocalTransform = Sequencer->GetFocusedMovieSceneSequenceTransform();
	
	for (FFrameTime EvalTime = InFrame; EvalTime <= OutFrame; EvalTime += Interval)
	{
		FFrameNumber KeyTime = FFrameRate::Snap(EvalTime, TickResolution, DisplayRate).FloorToFrame();
		FMovieSceneEvaluationRange Range(KeyTime * RootToLocalTransform.InverseLinearOnly(), TickResolution);

		Sequencer->SetGlobalTime(Range.GetTime());

		for (const FMovieSceneBindingProxy& ObjectBinding : ObjectBindings)
		{
			FGuid Guid = ObjectBinding.BindingID;

			for (TWeakObjectPtr<> RuntimeObject : Sequencer->FindObjectsInCurrentSequence(Guid) )
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

				BakeDataMap[Guid].KeyTimes.Add(KeyTime);
			}
		}
	}

	const bool bDisableSectionsAfterBaking = Sequencer->GetSequencerSettings()->GetDisableSectionsAfterBaking();

	for (TPair<FGuid, FBakeData>& BakeData : BakeDataMap)
	{
		FGuid Guid = BakeData.Key;

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
			LocalTranslations.SetNum(BakeData.Value.KeyTimes.Num());
			LocalRotations.SetNum(BakeData.Value.KeyTimes.Num());
			LocalScales.SetNum(BakeData.Value.KeyTimes.Num());

			for (int32 Counter = 0; Counter < BakeData.Value.KeyTimes.Num(); ++Counter)
			{
				FTransform LocalTransform(BakeData.Value.Rotations[Counter], BakeData.Value.Locations[Counter], BakeData.Value.Scales[Counter]);
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
				
			for (int32 Counter = 0; Counter < BakeData.Value.KeyTimes.Num(); ++Counter)
			{
				FFrameNumber KeyTime = BakeData.Value.KeyTimes[Counter];
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
		}
	}
	
	Sequencer->NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::MovieSceneStructureItemsChanged );
}

void ULevelSequenceEditorSubsystem::FixActorReferences()
{
	TSharedPtr<ISequencer> Sequencer = GetActiveSequencer();
	if (Sequencer == nullptr)
	{
		return;
	}

	UWorld* PlaybackContext = Cast<UWorld>(Sequencer->GetPlaybackContext());

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
	MenuBuilder.AddMenuEntry(FLevelSequenceEditorCommands::Get().AddActorsToBinding);
	MenuBuilder.AddMenuEntry(FLevelSequenceEditorCommands::Get().ReplaceBindingWithActors);
	MenuBuilder.AddMenuEntry(FLevelSequenceEditorCommands::Get().RemoveActorsFromBinding);
	MenuBuilder.AddMenuEntry(FLevelSequenceEditorCommands::Get().RemoveAllBindings);
	MenuBuilder.AddMenuEntry(FLevelSequenceEditorCommands::Get().RemoveInvalidBindings);

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
		
	for (UActorComponent* Component : Actor->GetComponents())
	{
		if (Component && ComponentPossessable && Component->GetName() != ComponentPossessable->GetName())
		{
			OutComponentNames.Add(Component->GetFName());
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

