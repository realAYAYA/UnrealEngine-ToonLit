// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions_EditorUtilityWidgetBlueprint.h"

#include "BlueprintEditor.h"
#include "Containers/UnrealString.h"
#include "Delegates/Delegate.h"
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "EditorUtilitySubsystem.h"
#include "EditorUtilityWidget.h"
#include "EditorUtilityWidgetBlueprint.h"
#include "Engine/Blueprint.h"
#include "Framework/Commands/UIAction.h"
#include "HAL/PlatformMisc.h"
#include "IBlutilityModule.h"
#include "Internationalization/Internationalization.h"
#include "Misc/MessageDialog.h"
#include "Modules/ModuleManager.h"
#include "SBlueprintDiff.h"
#include "Templates/Casts.h"
#include "Templates/ChooseClass.h"
#include "Templates/SubclassOf.h"
#include "Textures/SlateIcon.h"
#include "ToolMenuSection.h"
#include "Toolkits/IToolkit.h"
#include "UObject/Class.h"
#include "UObject/Object.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprintEditor.h"

#include <utility>

#define LOCTEXT_NAMESPACE "AssetTypeActions"

/////////////////////////////////////////////////////
// FAssetTypeActions_EditorUtilityWidget

FText FAssetTypeActions_EditorUtilityWidgetBlueprint::GetName() const
{
	return LOCTEXT("AssetTypeActions_EditorUtilityWidget", "Editor Utility Widget");
}

FColor FAssetTypeActions_EditorUtilityWidgetBlueprint::GetTypeColor() const
{
	return FColor(0, 169, 255);
}

UClass* FAssetTypeActions_EditorUtilityWidgetBlueprint::GetSupportedClass() const
{
	return UEditorUtilityWidgetBlueprint::StaticClass();
}

void FAssetTypeActions_EditorUtilityWidgetBlueprint::GetActions(const TArray<UObject*>& InObjects, FToolMenuSection& Section)
{
	auto Blueprints = GetTypedWeakObjectPtrs<UWidgetBlueprint>(InObjects);

	Section.AddMenuEntry(
		"EditorUtilityWidget_Edit",
		LOCTEXT("EditorUtilityWidget_Edit", "Run Editor Utility Widget"),
		LOCTEXT("EditorUtilityWidget_EditTooltip", "Opens the tab built by this Editor Utility Widget Blueprint."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP(this, &FAssetTypeActions_EditorUtilityWidgetBlueprint::ExecuteRun, Blueprints),
			FCanExecuteAction()
		)
	);

}

void FAssetTypeActions_EditorUtilityWidgetBlueprint::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor)
{
	EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

	for (UObject* Object : InObjects)
	{
		UBlueprint* Blueprint = Cast<UBlueprint>(Object);
		if (Blueprint && Blueprint->SkeletonGeneratedClass && Blueprint->GeneratedClass)
		{
			TSharedRef< FWidgetBlueprintEditor > NewBlueprintEditor(new FWidgetBlueprintEditor());

			TArray<UBlueprint*> Blueprints;
			Blueprints.Add(Blueprint);
			NewBlueprintEditor->InitWidgetBlueprintEditor(EToolkitMode::Standalone, nullptr, Blueprints, true);
		}
		else
		{
			FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("FailedToLoadEditorUtilityWidgetBlueprint", "Editor Utility Widget could not be loaded because it derives from an invalid class.\nCheck to make sure the parent class for this blueprint hasn't been removed!"));
		}
	}
}

uint32 FAssetTypeActions_EditorUtilityWidgetBlueprint::GetCategories()
{
	IBlutilityModule* BlutilityModule = FModuleManager::GetModulePtr<IBlutilityModule>("Blutility");
	return BlutilityModule->GetAssetCategory();
}

void FAssetTypeActions_EditorUtilityWidgetBlueprint::PerformAssetDiff(UObject* Asset1, UObject* Asset2, const struct FRevisionInfo& OldRevision, const struct FRevisionInfo& NewRevision) const
{
	UBlueprint* OldBlueprint = CastChecked<UBlueprint>(Asset1);
	UBlueprint* NewBlueprint = CastChecked<UBlueprint>(Asset2);

	// sometimes we're comparing different revisions of one single asset (other 
	// times we're comparing two completely separate assets altogether)
	bool bIsSingleAsset = (NewBlueprint->GetName() == OldBlueprint->GetName());

	FText WindowTitle = LOCTEXT("NamelessEditorUtilityWidgetBlueprintDiff", "Editor Utility Widget Blueprint Diff");
	// if we're diffing one asset against itself 
	if (bIsSingleAsset)
	{
		// identify the assumed single asset in the window's title
		WindowTitle = FText::Format(LOCTEXT("EditorUtilityWidgetBlueprintDiff", "{0} - Editor Utility Widget Blueprint Diff"), FText::FromString(NewBlueprint->GetName()));
	}

	SBlueprintDiff::CreateDiffWindow(WindowTitle, OldBlueprint, NewBlueprint, OldRevision, NewRevision);
}

void FAssetTypeActions_EditorUtilityWidgetBlueprint::ExecuteRun(FWeakBlueprintPointerArray InObjects)
{
	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		if (UWidgetBlueprint* Blueprint = Cast<UWidgetBlueprint>(*ObjIt))
		{
			if (Blueprint->GeneratedClass->IsChildOf(UEditorUtilityWidget::StaticClass()))
			{
				UEditorUtilityWidgetBlueprint* EditorWidget = Cast<UEditorUtilityWidgetBlueprint>(Blueprint);
				if (EditorWidget)
				{
					UEditorUtilitySubsystem* EditorUtilitySubsystem = GEditor->GetEditorSubsystem<UEditorUtilitySubsystem>();
					EditorUtilitySubsystem->SpawnAndRegisterTab(EditorWidget);
				}
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
