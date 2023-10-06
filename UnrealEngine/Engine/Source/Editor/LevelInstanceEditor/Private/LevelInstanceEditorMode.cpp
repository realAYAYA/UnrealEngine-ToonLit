// Copyright Epic Games, Inc. All Rights Reserved.

#include "LevelInstanceEditorMode.h"
#include "LevelInstanceEditorModeToolkit.h"
#include "LevelInstanceEditorModeCommands.h"
#include "Editor.h"
#include "Selection.h"
#include "EditorModes.h"
#include "Engine/World.h"
#include "LevelInstance/LevelInstanceSubsystem.h"
#include "LevelInstance/LevelInstanceInterface.h"
#include "LevelInstance/ILevelInstanceEditorModule.h"
#include "LevelEditorViewport.h"
#include "LevelEditorActions.h"
#include "EditorModeManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "LevelInstanceEditorMode"

FEditorModeID ULevelInstanceEditorMode::EM_LevelInstanceEditorModeId("EditMode.LevelInstance");

ULevelInstanceEditorMode::ULevelInstanceEditorMode()
	: UEdMode()
{
	Info = FEditorModeInfo(ULevelInstanceEditorMode::EM_LevelInstanceEditorModeId,
		LOCTEXT("LevelInstanceEditorModeName", "LevelInstanceEditorMode"),
		FSlateIcon(),
		false);

	bContextRestriction = true;
}

ULevelInstanceEditorMode::~ULevelInstanceEditorMode()
{
}

void ULevelInstanceEditorMode::OnPreBeginPIE(bool bSimulate)
{
	ExitModeCommand();
}

void ULevelInstanceEditorMode::UpdateEngineShowFlags()
{
	for (FLevelEditorViewportClient* LevelVC : GEditor->GetLevelViewportClients())
	{
		if (LevelVC && LevelVC->GetWorld())
		{
			if(ULevelInstanceSubsystem* LevelInstanceSubsystem = LevelVC->GetWorld()->GetSubsystem<ULevelInstanceSubsystem>())
			{
				const bool bEditingLevelInstance = IsContextRestrictedForWorld(LevelVC->GetWorld());
				// Make sure we update both Game/Editor showflags
				LevelVC->EngineShowFlags.EditingLevelInstance = bEditingLevelInstance;
				LevelVC->LastEngineShowFlags.EditingLevelInstance = bEditingLevelInstance;
			}
		}
	}
}

void ULevelInstanceEditorMode::Enter()
{
	UEdMode::Enter();

	UpdateEngineShowFlags();

	FEditorDelegates::PreBeginPIE.AddUObject(this, &ULevelInstanceEditorMode::OnPreBeginPIE);
}

void ULevelInstanceEditorMode::Exit()
{
	UEdMode::Exit();
		
	UpdateEngineShowFlags();

	bContextRestriction = true;

	FEditorDelegates::PreBeginPIE.RemoveAll(this);
}

void ULevelInstanceEditorMode::CreateToolkit()
{
	Toolkit = MakeShared<FLevelInstanceEditorModeToolkit>();
}

void ULevelInstanceEditorMode::ModeTick(float DeltaTime)
{
	Super::ModeTick(DeltaTime);

	UpdateEngineShowFlags();
}

bool ULevelInstanceEditorMode::IsCompatibleWith(FEditorModeID OtherModeID) const
{
	return (OtherModeID != FBuiltinEditorModes::EM_Foliage) && (OtherModeID != FBuiltinEditorModes::EM_Landscape);
}

void ULevelInstanceEditorMode::BindCommands()
{
	UEdMode::BindCommands();
	const TSharedRef<FUICommandList>& CommandList = Toolkit->GetToolkitCommands();
	const FLevelInstanceEditorModeCommands& Commands = FLevelInstanceEditorModeCommands::Get();

	CommandList->MapAction(
		Commands.ExitMode,
		FExecuteAction::CreateUObject(this, &ULevelInstanceEditorMode::ExitModeCommand),
		FCanExecuteAction::CreateLambda([&] 
		{ 
			// If some actors are selected make sure we don't interfere with the SelectNone command
			if(GEditor->GetSelectedActors()->Num() > 0)
			{
				const FInputChord& SelectNonePrimary = FLevelEditorCommands::Get().SelectNone->GetActiveChord(EMultipleKeyBindingIndex::Primary).Get();
				if (SelectNonePrimary.IsValidChord() && Commands.ExitMode->HasActiveChord(SelectNonePrimary))
				{
					return false;
				}

				const FInputChord& SelectNoneSecondary = FLevelEditorCommands::Get().SelectNone->GetActiveChord(EMultipleKeyBindingIndex::Secondary).Get();
				if (SelectNoneSecondary.IsValidChord() && Commands.ExitMode->HasActiveChord(SelectNoneSecondary))
				{
					return false;
				}
			}

			return true;
		}));

	CommandList->MapAction(
		Commands.ToggleContextRestriction,
		FExecuteAction::CreateUObject(this, &ULevelInstanceEditorMode::ToggleContextRestrictionCommand),
		FCanExecuteAction(),
		FIsActionChecked::CreateUObject(this, &ULevelInstanceEditorMode::IsContextRestrictionCommandEnabled));
}

bool ULevelInstanceEditorMode::IsSelectionDisallowed(AActor* InActor, bool bInSelection) const
{
	UWorld* World = InActor->GetWorld();
	const bool bRestrict = bInSelection && IsContextRestrictedForWorld(World);

	if (bRestrict)
	{
		check(World);
		if (ILevelInstanceInterface* LevelInstance = Cast<ILevelInstanceInterface>(InActor))
		{
			if (LevelInstance->IsEditing())
			{
				return false;
			}
		}

		if (ULevelInstanceSubsystem* LevelInstanceSubsystem = World->GetSubsystem<ULevelInstanceSubsystem>())
		{
			ILevelInstanceInterface* EditingLevelInstance = LevelInstanceSubsystem->GetEditingLevelInstance();
			ILevelInstanceInterface* LevelInstance = LevelInstanceSubsystem->GetParentLevelInstance(InActor);
			// Allow selection on actors that are part of the currently edited Level Instance hierarchy because AActor::GetRootSelectionParent() will eventually
			// Bubble up the selection to its parent.
			while (LevelInstance != nullptr)
			{
				if (LevelInstance == EditingLevelInstance)
				{
					return false;
				}

				LevelInstance = LevelInstanceSubsystem->GetParentLevelInstance(CastChecked<AActor>(LevelInstance));
			}

			return EditingLevelInstance != nullptr;
		}
	}

	return bRestrict;
}

void ULevelInstanceEditorMode::ExitModeCommand()
{	
	// Ignore command when any modal window is open
	if (FSlateApplication::IsInitialized() && FSlateApplication::Get().GetActiveModalWindow().IsValid())
	{
		return;
	}

	if (ILevelInstanceEditorModule* EditorModule = FModuleManager::GetModulePtr<ILevelInstanceEditorModule>("LevelInstanceEditor"))
	{
		EditorModule->BroadcastTryExitEditorMode();
	}
}

void ULevelInstanceEditorMode::ToggleContextRestrictionCommand()
{
	bContextRestriction = !bContextRestriction;

	UpdateEngineShowFlags();
}

bool ULevelInstanceEditorMode::IsContextRestrictionCommandEnabled() const
{
	return bContextRestriction;
}

bool ULevelInstanceEditorMode::IsContextRestrictedForWorld(UWorld* InWorld) const
{
	if (ULevelInstanceSubsystem* LevelInstanceSubsystem = InWorld? InWorld->GetSubsystem<ULevelInstanceSubsystem>() : nullptr)
	{
		if (ILevelInstanceInterface* EditingLevelInstance = LevelInstanceSubsystem->GetEditingLevelInstance())
		{
			return bContextRestriction && LevelInstanceSubsystem->GetLevelInstanceLevel(EditingLevelInstance) == InWorld->GetCurrentLevel();
		}
	}

	return false;
}

#undef LOCTEXT_NAMESPACE
