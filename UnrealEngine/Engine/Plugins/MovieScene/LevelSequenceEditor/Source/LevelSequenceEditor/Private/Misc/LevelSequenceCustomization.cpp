// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/LevelSequenceCustomization.h"

#include "ClassViewerModule.h"
#include "Engine/LevelStreaming.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "ISequencer.h"
#include "ISequencerModule.h"
#include "LevelSequenceDirector.h"
#include "LevelSequenceEditorCommands.h"
#include "LevelSequenceFBXInterop.h"
#include "Modules/ModuleManager.h"
#include "MovieScene.h"
#include "MVVM/Extensions/IObjectBindingExtension.h"
#include "MVVM/Selection/Selection.h"
#include "MVVM/ViewModels/ObjectBindingModel.h"
#include "MVVM/ViewModels/SequencerEditorViewModel.h"
#include "ScopedTransaction.h"
#include "SequencerCommands.h"
#include "SequencerUtilities.h"
#include "Tracks/MovieSceneSpawnTrack.h"

#define LOCTEXT_NAMESPACE "LevelSequenceCustomization"

namespace UE::Sequencer
{

struct FMovieSceneSpawnableFlagCheckState
{
	TSharedPtr<FSequencerEditorViewModel> EditorViewModel;
	UMovieScene* MovieScene;
	bool FMovieSceneSpawnable::* PtrToFlag;

	ECheckBoxState operator()() const
	{
		ECheckBoxState CheckState = ECheckBoxState::Undetermined;
		for (TViewModelPtr<IObjectBindingExtension> ObjectBinding : EditorViewModel->GetSelection()->Outliner.Filter<IObjectBindingExtension>())
		{
			FMovieSceneSpawnable* SelectedSpawnable = MovieScene->FindSpawnable(ObjectBinding->GetObjectGuid());
			if (SelectedSpawnable)
			{
				if (CheckState != ECheckBoxState::Undetermined && SelectedSpawnable->*PtrToFlag != (CheckState == ECheckBoxState::Checked))
				{
					return ECheckBoxState::Undetermined;
				}
				CheckState = SelectedSpawnable->*PtrToFlag ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			}
		}
		return CheckState;
	}
};

struct FMovieSceneSpawnableFlagToggler
{
	TSharedPtr<FSequencerEditorViewModel> EditorViewModel;
	UMovieScene* MovieScene;
	bool FMovieSceneSpawnable::* PtrToFlag;
	FText TransactionText;

	void operator()() const
	{
		FScopedTransaction Transaction(TransactionText);

		const ECheckBoxState CheckState = FMovieSceneSpawnableFlagCheckState{ EditorViewModel, MovieScene, PtrToFlag }();

		MovieScene->Modify();
		for (TViewModelPtr<IObjectBindingExtension> ObjectBinding : EditorViewModel->GetSelection()->Outliner.Filter<IObjectBindingExtension>())
		{
			FMovieSceneSpawnable* SelectedSpawnable = MovieScene->FindSpawnable(ObjectBinding->GetObjectGuid());
			if (SelectedSpawnable)
			{
				SelectedSpawnable->*PtrToFlag = (CheckState == ECheckBoxState::Unchecked);
			}
		}
	}
};

void FLevelSequenceCustomization::RegisterSequencerCustomization(FSequencerCustomizationBuilder& Builder)
{
	WeakSequencer = Builder.GetSequencer().AsShared();

	const FLevelSequenceEditorCommands& Commands = FLevelSequenceEditorCommands::Get();

	// Build the extender for the actions menu.
	ActionsMenuCommandList = MakeShared<FUICommandList>().ToSharedPtr();
	ActionsMenuCommandList->MapAction(
		Commands.ImportFBX,
		FExecuteAction::CreateRaw( this, &FLevelSequenceCustomization::ImportFBX ),
		FCanExecuteAction::CreateLambda( [] { return true; } ) );
	ActionsMenuCommandList->MapAction(
		Commands.ExportFBX,
		FExecuteAction::CreateRaw( this, &FLevelSequenceCustomization::ExportFBX ),
		FCanExecuteAction::CreateLambda( [] { return true; } ) );

	ActionsMenuExtender = MakeShared<FExtender>();
	ActionsMenuExtender->AddMenuExtension(
			"SequenceOptions", EExtensionHook::First, nullptr,
			FMenuExtensionDelegate::CreateRaw(this, &FLevelSequenceCustomization::ExtendActionsMenu));

	ISequencerModule& SequencerModule = FModuleManager::LoadModuleChecked<ISequencerModule>("Sequencer");
	SequencerModule.GetActionsMenuExtensibilityManager()->AddExtender(ActionsMenuExtender);

	// Add a customization callback for the object binding context menu.
	FSequencerCustomizationInfo Customization;
	Customization.OnBuildObjectBindingContextMenu = FOnGetSequencerMenuExtender::CreateRaw(this, &FLevelSequenceCustomization::CreateObjectBindingContextMenuExtender);
	Builder.AddCustomization(Customization);
}

void FLevelSequenceCustomization::UnregisterSequencerCustomization()
{
	ISequencerModule& SequencerModule = FModuleManager::LoadModuleChecked<ISequencerModule>("Sequencer");
	SequencerModule.GetActionsMenuExtensibilityManager()->RemoveExtender(ActionsMenuExtender);

	ActionsMenuCommandList = nullptr;
	WeakSequencer = nullptr;
}

void FLevelSequenceCustomization::ExtendActionsMenu(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.PushCommandList(ActionsMenuCommandList.ToSharedRef());
	{
		const FLevelSequenceEditorCommands& Commands = FLevelSequenceEditorCommands::Get();
		
		MenuBuilder.AddMenuEntry(
				LOCTEXT("SaveAs", "Save As..."),
				LOCTEXT("SaveAsTooltip", "Saves the current sequence under a different name"),
				FSlateIcon(Commands.GetStyleSetName(), "LevelSequenceEditor.SaveAs"),
				FUIAction(FExecuteAction::CreateRaw(this, &FLevelSequenceCustomization::OnSaveMovieSceneAsClicked)));

		MenuBuilder.AddMenuEntry(Commands.ImportFBX);
		MenuBuilder.AddMenuEntry(Commands.ExportFBX);
	}
	MenuBuilder.PopCommandList();
}

void FLevelSequenceCustomization::OnSaveMovieSceneAsClicked()
{
	FSequencerUtilities::SaveCurrentMovieSceneAs(WeakSequencer.Pin().ToSharedRef());
}

void FLevelSequenceCustomization::ImportFBX()
{
	FLevelSequenceFBXInterop Interop(WeakSequencer.Pin());
	Interop.ImportFBX();
}

void FLevelSequenceCustomization::ExportFBX()
{
	FLevelSequenceFBXInterop Interop(WeakSequencer.Pin());
	Interop.ExportFBX();
}

TSharedPtr<FExtender> FLevelSequenceCustomization::CreateObjectBindingContextMenuExtender(FViewModelPtr InViewModel)
{
	TSharedRef<FExtender> Extender = MakeShared<FExtender>();
	TSharedPtr<FObjectBindingModel> ObjectBindingModel = InViewModel->CastThisShared<FObjectBindingModel>();
	Extender->AddMenuExtension(
			"ObjectBindingActions", EExtensionHook::Before, nullptr,
			FMenuExtensionDelegate::CreateRaw(this, &FLevelSequenceCustomization::ExtendObjectBindingContextMenu, ObjectBindingModel));
	return Extender.ToSharedPtr();
}

void FLevelSequenceCustomization::ExtendObjectBindingContextMenu(FMenuBuilder& MenuBuilder, TSharedPtr<FObjectBindingModel> ObjectBindingModel)
{
	TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
	TSharedPtr<FSequencerEditorViewModel> EditorViewModel = Sequencer->GetViewModel();

	FGuid ObjectBindingID = ObjectBindingModel->GetObjectGuid();
	UMovieScene* MovieScene = Sequencer->GetFocusedMovieSceneSequence()->GetMovieScene();

	if (!MovieScene || !ObjectBindingID.IsValid())
	{
		return;
	}

	FMovieSceneSpawnable* Spawnable = MovieScene->FindSpawnable(ObjectBindingID);

	if (Spawnable)
	{
		MenuBuilder.BeginSection("Spawnable", LOCTEXT("SpawnableMenuSectionName", "Spawnable"));

		MenuBuilder.AddSubMenu(
			LOCTEXT("OwnerLabel", "Spawned Object Owner"),
			LOCTEXT("OwnerTooltip", "Specifies how the spawned object is to be owned"),
			FNewMenuDelegate::CreateRaw(this, &FLevelSequenceCustomization::AddSpawnOwnershipMenu, ObjectBindingModel)
		);

		MenuBuilder.AddSubMenu(
			LOCTEXT("SubLevelLabel", "Spawnable Level"),
			LOCTEXT("SubLevelTooltip", "Specifies which level the spawnable should be spawned into"),
			FNewMenuDelegate::CreateRaw(this, &FLevelSequenceCustomization::AddSpawnLevelMenu, ObjectBindingModel)
		);

		MenuBuilder.AddSubMenu(
			LOCTEXT("ChangeClassLabel", "Change Class"),
			LOCTEXT("ChangeClassTooltip", "Change the class (object template) that this spawns from"),
			FNewMenuDelegate::CreateRaw(this, &FLevelSequenceCustomization::AddChangeClassMenu, ObjectBindingModel));

		MenuBuilder.AddMenuEntry(
			LOCTEXT("ContinuouslyRespawn", "Continuously Respawn"),
			LOCTEXT("ContinuouslyRespawnTooltip", "When enabled, this spawnable will always be respawned if it gets destroyed externally. When disabled, this object will only ever be spawned once for each spawn key even if it is destroyed externally"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda(FMovieSceneSpawnableFlagToggler{ EditorViewModel, MovieScene, &FMovieSceneSpawnable::bContinuouslyRespawn, LOCTEXT("ContinuouslyRespawnTransaction", "Set Continuously Respawn") }),
				FCanExecuteAction(),
				FGetActionCheckState::CreateLambda(FMovieSceneSpawnableFlagCheckState{ EditorViewModel, MovieScene, &FMovieSceneSpawnable::bContinuouslyRespawn })
			),
			NAME_None,
			EUserInterfaceActionType::ToggleButton
		);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("NetAddressable", "Net Addressable"),
			LOCTEXT("NetAddressableTooltip", "When enabled, this spawnable will be spawned using a unique name that allows it to be addressed by the server and client (useful for relative movement calculations on spawned props)"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda(FMovieSceneSpawnableFlagToggler{ EditorViewModel, MovieScene, &FMovieSceneSpawnable::bNetAddressableName, LOCTEXT("NetAddressableTransaction", "Set Net Addressable") }),
				FCanExecuteAction(),
				FGetActionCheckState::CreateLambda(FMovieSceneSpawnableFlagCheckState{ EditorViewModel, MovieScene, &FMovieSceneSpawnable::bNetAddressableName })
			),
			NAME_None,
			EUserInterfaceActionType::ToggleButton
		);

		MenuBuilder.AddMenuEntry(FSequencerCommands::Get().SaveCurrentSpawnableState);
		MenuBuilder.AddMenuEntry(FSequencerCommands::Get().ConvertToPossessable);

		MenuBuilder.AddSubMenu(
				LOCTEXT("DynamicSpawn", "Dynamic Spawn"),
				LOCTEXT("DynamicSpawnTooltip", "Specify a Blueprint method that will spawn or otherwise acquire a compatible actor for this binding"),
				FNewMenuDelegate::CreateRaw(this, &FLevelSequenceCustomization::AddDynamicSpawnMenu, ObjectBindingModel));

		MenuBuilder.EndSection();
	}
	else
	{
		MenuBuilder.BeginSection("Possessable");

		MenuBuilder.AddMenuEntry(FSequencerCommands::Get().ConvertToSpawnable);

		if (UMovieScene::IsTrackClassAllowed(ULevelSequenceDirector::StaticClass()))
		{
			MenuBuilder.AddSubMenu(
					LOCTEXT("DynamicPossession", "Dynamic Possession"),
					LOCTEXT("DynamicPossessionTooltip", "Specify a Blueprint method that will find a compatible actor for this binding"),
					FNewMenuDelegate::CreateRaw(this, &FLevelSequenceCustomization::AddDynamicPossessionMenu, ObjectBindingModel));
		}

		MenuBuilder.EndSection();
	}

	MenuBuilder.BeginSection("Import/Export", LOCTEXT("ImportExportMenuSectionName", "Import/Export"));

	MenuBuilder.AddMenuEntry(
		LOCTEXT("ImportFBX", "Import..."),
		LOCTEXT("ImportFBXTooltip", "Import FBX animation to this object"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([=] {
					FLevelSequenceFBXInterop Interop(EditorViewModel->GetSequencer());
					Interop.ImportFBXOntoSelectedNodes();
				})
		));

	MenuBuilder.AddMenuEntry(
		LOCTEXT("ExportFBX", "Export..."),
		LOCTEXT("ExportFBXTooltip", "Export FBX animation from this object"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([=] {
					FLevelSequenceFBXInterop Interop(EditorViewModel->GetSequencer());
					Interop.ExportFBX();
				})
		));

	MenuBuilder.EndSection();
}

void FLevelSequenceCustomization::AddSpawnOwnershipMenu(FMenuBuilder& MenuBuilder, TSharedPtr<FObjectBindingModel> ObjectBindingModel)
{
	TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
	TSharedPtr<FSequencerEditorViewModel> EditorViewModel = Sequencer->GetViewModel();

	FGuid ObjectBindingID = ObjectBindingModel->GetObjectGuid();
	UMovieScene* MovieScene = Sequencer->GetFocusedMovieSceneSequence()->GetMovieScene();

	FMovieSceneSpawnable* Spawnable = MovieScene->FindSpawnable(ObjectBindingID);
	if (!Spawnable)
	{
		return;
	}

	auto Callback = [=](ESpawnOwnership NewOwnership)
	{
		FScopedTransaction Transaction(LOCTEXT("SetSpawnOwnership", "Set Spawnable Ownership"));

		MovieScene->Modify();

		for (TViewModelPtr<IObjectBindingExtension> ObjectBinding : Sequencer->GetViewModel()->GetSelection()->Outliner.Filter<IObjectBindingExtension>())
		{
			FMovieSceneSpawnable* Spawnable = MovieScene->FindSpawnable(ObjectBinding->GetObjectGuid());

			Spawnable->SetSpawnOwnership(NewOwnership);

			// Overwrite the completion state for all spawn sections to ensure the expected behaviour.
			EMovieSceneCompletionMode NewCompletionMode = NewOwnership == ESpawnOwnership::InnerSequence ? EMovieSceneCompletionMode::RestoreState : EMovieSceneCompletionMode::KeepState;

			// Make all spawn sections retain state
			UMovieSceneSpawnTrack* SpawnTrack = MovieScene->FindTrack<UMovieSceneSpawnTrack>(ObjectBinding->GetObjectGuid());
			if (SpawnTrack)
			{
				for (UMovieSceneSection* Section : SpawnTrack->GetAllSections())
				{
					Section->Modify();
					Section->EvalOptions.CompletionMode = NewCompletionMode;
				}
			}
		}
	};

	MenuBuilder.AddMenuEntry(
		LOCTEXT("ThisSequence_Label", "This Sequence"),
		LOCTEXT("ThisSequence_Tooltip", "Indicates that this sequence will own the spawned object. The object will be destroyed at the end of the sequence."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda(Callback, ESpawnOwnership::InnerSequence),
			FCanExecuteAction(),
			FIsActionChecked::CreateLambda([=] { return Spawnable->GetSpawnOwnership() == ESpawnOwnership::InnerSequence; })
		),
		NAME_None,
		EUserInterfaceActionType::ToggleButton
	);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("RootSequence_Label", "Root Sequence"),
		LOCTEXT("RootSequence_Tooltip", "Indicates that the outermost sequence will own the spawned object. The object will be destroyed when the outermost sequence stops playing."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda(Callback, ESpawnOwnership::RootSequence),
			FCanExecuteAction(),
			FIsActionChecked::CreateLambda([=] { return Spawnable->GetSpawnOwnership() == ESpawnOwnership::RootSequence; })
		),
		NAME_None,
		EUserInterfaceActionType::ToggleButton
	);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("External_Label", "External"),
		LOCTEXT("External_Tooltip", "Indicates this object's lifetime is managed externally once spawned. It will not be destroyed by sequencer."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda(Callback, ESpawnOwnership::External),
			FCanExecuteAction(),
			FIsActionChecked::CreateLambda([=] { return Spawnable->GetSpawnOwnership() == ESpawnOwnership::External; })
		),
		NAME_None,
		EUserInterfaceActionType::ToggleButton
	);
}

void FLevelSequenceCustomization::AddSpawnLevelMenu(FMenuBuilder& MenuBuilder, TSharedPtr<FObjectBindingModel> ObjectBindingModel)
{
	TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
	TSharedPtr<FSequencerEditorViewModel> EditorViewModel = Sequencer->GetViewModel();

	FGuid ObjectBindingID = ObjectBindingModel->GetObjectGuid();
	UMovieScene* MovieScene = Sequencer->GetFocusedMovieSceneSequence()->GetMovieScene();

	FMovieSceneSpawnable* Spawnable = MovieScene->FindSpawnable(ObjectBindingID);
	if (!Spawnable)
	{
		return;
	}

	MenuBuilder.AddMenuEntry(
		NSLOCTEXT("UnrealEd", "PersistentLevel", "Persistent Level"),
		NSLOCTEXT("UnrealEd", "PersistentLevel", "Persistent Level"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateRaw(this, &FLevelSequenceCustomization::SetSelectedNodesSpawnableLevel, ObjectBindingModel, FName(NAME_None)),
			FCanExecuteAction(),
			FIsActionChecked::CreateLambda([=] { return Spawnable->GetLevelName() == NAME_None; })
		),
		NAME_None,
		EUserInterfaceActionType::ToggleButton
	);

	UWorld* World = Sequencer->GetPlaybackContext()->GetWorld();
	if (!World)
	{
		return;
	}

	for (ULevelStreaming* LevelStreaming : World->GetStreamingLevels())
	{
		if (LevelStreaming)
		{
			FName LevelName = FPackageName::GetShortFName(LevelStreaming->GetWorldAssetPackageFName());

			MenuBuilder.AddMenuEntry(
				FText::FromName(LevelName),
				FText::FromName(LevelName),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateRaw(this, &FLevelSequenceCustomization::SetSelectedNodesSpawnableLevel, ObjectBindingModel, LevelName),
					FCanExecuteAction(),
					FIsActionChecked::CreateLambda([=] { return Spawnable->GetLevelName() == LevelName; })
				),
				NAME_None,
				EUserInterfaceActionType::ToggleButton
			);
		}
	}
}

void FLevelSequenceCustomization::SetSelectedNodesSpawnableLevel(TSharedPtr<FObjectBindingModel> ObjectBindingModel, FName InLevelName)
{
	TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
	TSharedPtr<FSequencerEditorViewModel> EditorViewModel = Sequencer->GetViewModel();
	UMovieScene* MovieScene = Sequencer->GetFocusedMovieSceneSequence()->GetMovieScene();

	if (MovieScene->IsReadOnly())
	{
		FSequencerUtilities::ShowReadOnlyError();
		return;
	}

	const FScopedTransaction Transaction( LOCTEXT("SetSpawnableLevel", "Set Spawnable Level") );

	MovieScene->Modify();

	TArray<FMovieSceneSpawnable*> Spawnables;

	for (TViewModelPtr<IObjectBindingExtension> ObjectBindingNode : EditorViewModel->GetSelection()->Outliner.Filter<IObjectBindingExtension>())
	{
		FMovieSceneSpawnable* Spawnable = MovieScene->FindSpawnable(ObjectBindingNode->GetObjectGuid());
		if (Spawnable)
		{
			Spawnable->SetLevelName(InLevelName);
		}
	}
}

void FLevelSequenceCustomization::AddChangeClassMenu(FMenuBuilder& MenuBuilder, TSharedPtr<FObjectBindingModel> ObjectBindingModel)
{
	TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
	TSharedPtr<FSequencerEditorViewModel> EditorViewModel = Sequencer->GetViewModel();

	FGuid ObjectBindingID = ObjectBindingModel->GetObjectGuid();
	UMovieScene* MovieScene = Sequencer->GetFocusedMovieSceneSequence()->GetMovieScene();

	FMovieSceneSpawnable* Spawnable = MovieScene->FindSpawnable(ObjectBindingID);
	if (!Spawnable)
	{
		return;
	}

	FClassViewerModule& ClassViewerModule = FModuleManager::LoadModuleChecked<FClassViewerModule>("ClassViewer");

	FClassViewerInitializationOptions Options;
	Options.Mode = EClassViewerMode::ClassPicker;
	Options.bIsActorsOnly = true;
	Options.bIsPlaceableOnly = true;

	const UClass* ClassForObjectBinding = ObjectBindingModel->FindObjectClass();
	if (ClassForObjectBinding)
	{
		Options.ViewerTitleString = FText::FromString(TEXT("Change from: ") + ClassForObjectBinding->GetFName().ToString());
	}
	else
	{
		Options.ViewerTitleString = FText::FromString(TEXT("Change from: (empty)"));
	}

	MenuBuilder.AddWidget(
		SNew(SBox)
		.MinDesiredWidth(300.0f)
		.MaxDesiredHeight(400.0f)
		[
			ClassViewerModule.CreateClassViewer(Options, FOnClassPicked::CreateRaw(
				this, &FLevelSequenceCustomization::HandleTemplateActorClassPicked, ObjectBindingModel))
		],
		FText(), true, false
	);
}

void FLevelSequenceCustomization::HandleTemplateActorClassPicked(UClass* ChosenClass, TSharedPtr<FObjectBindingModel> ObjectBindingModel)
{
	TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
	UMovieScene* MovieScene = Sequencer->GetFocusedMovieSceneSequence()->GetMovieScene();

	FGuid ObjectBindingID = ObjectBindingModel->GetObjectGuid();

	FSlateApplication::Get().DismissAllMenus();

	FMovieSceneSpawnable* Spawnable = MovieScene->FindSpawnable(ObjectBindingID);
	if (!Spawnable)
	{
		return;
	}

	FScopedTransaction Transaction(LOCTEXT("ChangeClass", "Change Class"));

	MovieScene->Modify();

	TValueOrError<FNewSpawnable, FText> Result = Sequencer->GetSpawnRegister().CreateNewSpawnableType(*ChosenClass, *MovieScene, nullptr);
	if (Result.IsValid())
	{
		Spawnable->SetObjectTemplate(Result.GetValue().ObjectTemplate);

		Sequencer->GetSpawnRegister().DestroySpawnedObject(Spawnable->GetGuid(), Sequencer->GetFocusedTemplateID(), *Sequencer.Get());
		Sequencer->ForceEvaluate();
	}
}

void FLevelSequenceCustomization::AddDynamicSpawnMenu(FMenuBuilder& MenuBuilder, TSharedPtr<FObjectBindingModel> ObjectBindingModel)
{
	TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
	UMovieScene* MovieScene = Sequencer->GetFocusedMovieSceneSequence()->GetMovieScene();

	FGuid ObjectBindingID = ObjectBindingModel->GetObjectGuid();
	FMovieSceneSpawnable* Spawnable = MovieScene->FindSpawnable(ObjectBindingID);
	if (!Spawnable)
	{
		return;
	}

	ObjectBindingModel->AddDynamicBindingMenu(MenuBuilder, Spawnable->DynamicBinding);
}

void FLevelSequenceCustomization::AddDynamicPossessionMenu(FMenuBuilder& MenuBuilder, TSharedPtr<FObjectBindingModel> ObjectBindingModel)
{
	TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
	UMovieScene* MovieScene = Sequencer->GetFocusedMovieSceneSequence()->GetMovieScene();

	FGuid ObjectBindingID = ObjectBindingModel->GetObjectGuid();
	FMovieScenePossessable* Possessable = MovieScene->FindPossessable(ObjectBindingID);
	if (!Possessable)
	{
		return;
	}

	ObjectBindingModel->AddDynamicBindingMenu(MenuBuilder, Possessable->DynamicBinding);
}

} // namespace UE::Sequencer

#undef LOCTEXT_NAMESPACE

