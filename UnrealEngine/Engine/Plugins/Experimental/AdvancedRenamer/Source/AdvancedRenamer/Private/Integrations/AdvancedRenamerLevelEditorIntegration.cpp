// Copyright Epic Games, Inc. All Rights Reserved.

#include "Integrations/AdvancedRenamerLevelEditorIntegration.h"
#include "AdvancedRenamerCommands.h"
#include "Containers/Array.h"
#include "Delegates/IDelegateInstance.h"
#include "EditorModeManager.h"
#include "GameFramework/Actor.h"
#include "IAdvancedRenamerModule.h"
#include "ILevelEditor.h"
#include "LevelEditor.h"
#include "Selection.h"
#include "Templates/SharedPointer.h"

namespace UE::AdvancedRenamer::Private
{
	FDelegateHandle LevelEditorCreatedDelegateHandle;

	TArray<AActor*> GetSelectedActors(TSharedRef<ILevelEditor> InLevelEditor)
	{
		USelection* ActorSelection = InLevelEditor->GetEditorModeManager().GetSelectedActors();

		if (!ActorSelection)
		{
			return {};
		}

		TArray<AActor*> SelectedActors;
		int32 Count = ActorSelection->GetSelectedObjects<AActor>(SelectedActors);

		return SelectedActors;
	}

	bool CanOpenAdvancedRenamer(TWeakPtr<ILevelEditor> InLevelEditorWeak)
	{
		TSharedPtr<ILevelEditor> LevelEditor = InLevelEditorWeak.Pin();

		if (!LevelEditor)
		{
			return false;
		}

		TArray<AActor*> SelectedActors = GetSelectedActors(LevelEditor.ToSharedRef());

		for (AActor* SelectedActor : SelectedActors)
		{
			if (IsValid(SelectedActor))
			{
				return true;
			}
		}

		return false;
	}

	void RenameSelectedActors(TWeakPtr<ILevelEditor> InLevelEditorWeak)
	{
		TSharedPtr<ILevelEditor> LevelEditor = InLevelEditorWeak.Pin();

		if (!LevelEditor)
		{
			return;
		}

		TArray<AActor*> SelectedActors = GetSelectedActors(LevelEditor.ToSharedRef());

		IAdvancedRenamerModule::Get().OpenAdvancedRenamerForActors(SelectedActors, StaticCastSharedPtr<IToolkitHost>(LevelEditor));
	}

	void RenameSharedClassActors(TWeakPtr<ILevelEditor> InLevelEditorWeak)
	{
		TSharedPtr<ILevelEditor> LevelEditor = InLevelEditorWeak.Pin();

		if (!LevelEditor)
		{
			return;
		}

		IAdvancedRenamerModule& AdvancedRenamerModule = IAdvancedRenamerModule::Get();

		TArray<AActor*> SelectedActors = GetSelectedActors(LevelEditor.ToSharedRef());
		SelectedActors = AdvancedRenamerModule.GetActorsSharingClassesInWorld(SelectedActors);

		AdvancedRenamerModule.OpenAdvancedRenamerForActors(SelectedActors, StaticCastSharedPtr<IToolkitHost>(LevelEditor));
	}

	void OnLevelEditorCreated(TSharedPtr<ILevelEditor> InLevelEditor)
	{
		const FAdvancedRenamerCommands& AdvRenCommands = FAdvancedRenamerCommands::Get();

		InLevelEditor->GetLevelEditorActions()->MapAction(
			AdvRenCommands.RenameSelectedActors,
			FExecuteAction::CreateStatic(&RenameSelectedActors, InLevelEditor.ToWeakPtr()),
			FCanExecuteAction::CreateStatic(&CanOpenAdvancedRenamer, InLevelEditor.ToWeakPtr())
		);

		InLevelEditor->GetLevelEditorActions()->MapAction(
			AdvRenCommands.RenameSharedClassActors,
			FExecuteAction::CreateStatic(&RenameSharedClassActors, InLevelEditor.ToWeakPtr()),
			FCanExecuteAction::CreateStatic(&CanOpenAdvancedRenamer, InLevelEditor.ToWeakPtr())
		);
	}
}

void FAdvancedRenamerLevelEditorIntegration::Initialize()
{
	using namespace UE::AdvancedRenamer::Private;

	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
	LevelEditorCreatedDelegateHandle = LevelEditorModule.OnLevelEditorCreated().AddStatic(&OnLevelEditorCreated);

	const FAdvancedRenamerCommands& AdvRenCommands = FAdvancedRenamerCommands::Get();

	static const TArray<FName, TInlineAllocator<2>> Menus = {
		"LevelEditor.ActorContextMenu.EditSubMenu",
		"LevelEditor.LevelEditorSceneOutliner.ContextMenu.EditSubMenu"
	};

	const TAttribute<FText> TextAttribute;
	const FSlateIcon RenameIcon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "GenericCommands.Rename");

	for (const FName& Menu : Menus)
	{
		UToolMenu* ToolMenu = UToolMenus::Get()->ExtendMenu(Menu);
		FToolMenuSection& Section = ToolMenu->FindOrAddSection(NAME_None);

		Section.AddMenuEntry(
			AdvRenCommands.RenameSelectedActors,
			TextAttribute,
			TextAttribute,
			RenameIcon
		);

		Section.AddMenuEntry(
			AdvRenCommands.RenameSharedClassActors,
			TextAttribute,
			TextAttribute,
			RenameIcon
		);
	}
}

void FAdvancedRenamerLevelEditorIntegration::Shutdown()
{
	using namespace UE::AdvancedRenamer::Private;

	if (FLevelEditorModule* LevelEditorModule = FModuleManager::LoadModulePtr<FLevelEditorModule>("LevelEditor"))
	{
		LevelEditorModule->OnLevelEditorCreated().Remove(LevelEditorCreatedDelegateHandle);
	}
}
