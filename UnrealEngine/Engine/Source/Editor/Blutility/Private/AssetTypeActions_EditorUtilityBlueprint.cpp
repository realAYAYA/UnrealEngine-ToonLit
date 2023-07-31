// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions_EditorUtilityBlueprint.h"

#include "Containers/UnrealString.h"
#include "ContentBrowserModule.h"
#include "Delegates/Delegate.h"
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "EditorUtilityBlueprint.h"
#include "EditorUtilityBlueprintFactory.h"
#include "EditorUtilitySubsystem.h"
#include "Engine/Blueprint.h"
#include "Framework/Commands/UIAction.h"
#include "HAL/PlatformMisc.h"
#include "IBlutilityModule.h"
#include "IContentBrowserSingleton.h"
#include "Internationalization/Internationalization.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/MessageDialog.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "Templates/SubclassOf.h"
#include "Textures/SlateIcon.h"
#include "ToolMenuSection.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

#include <utility>

class UClass;
class UObject;

#define LOCTEXT_NAMESPACE "AssetTypeActions"

/////////////////////////////////////////////////////
// FAssetTypeActions_EditorUtilityBlueprint

FText FAssetTypeActions_EditorUtilityBlueprint::GetName() const
{
	return LOCTEXT("AssetTypeActions_EditorUtilityBlueprintUpdate", "Editor Utility Blueprint");
}

FColor FAssetTypeActions_EditorUtilityBlueprint::GetTypeColor() const
{
	return FColor(0, 169, 255);
}

UClass* FAssetTypeActions_EditorUtilityBlueprint::GetSupportedClass() const
{
	return UEditorUtilityBlueprint::StaticClass();
}

void FAssetTypeActions_EditorUtilityBlueprint::GetActions(const TArray<UObject*>& InObjects, FToolMenuSection& Section)
{
	TArray<TWeakObjectPtr<UEditorUtilityBlueprint>> Blueprints = GetTypedWeakObjectPtrs<UEditorUtilityBlueprint>(InObjects);

	UEditorUtilitySubsystem* EditorUtilitySubsystem = GEditor->GetEditorSubsystem<UEditorUtilitySubsystem>();

	bool bHasRunnable = false;
	for (TWeakObjectPtr<UEditorUtilityBlueprint>& It : Blueprints)
	{
		if (UEditorUtilityBlueprint* Asset = It.Get())
		{
			if (EditorUtilitySubsystem->CanRun(Asset))
			{
				bHasRunnable = true;
				break;
			}
		}
	}

	if (bHasRunnable)
	{
		Section.AddMenuEntry(
			"EditorUtility_Run",
			LOCTEXT("EditorUtility_Run", "Run Editor Utility Blueprint"),
			LOCTEXT("EditorUtility_RunTooltip", "Runs this Editor Utility Blueprint."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP(this, &FAssetTypeActions_EditorUtilityBlueprint::ExecuteRun, Blueprints)
			)
		);
	}
}

uint32 FAssetTypeActions_EditorUtilityBlueprint::GetCategories()
{
	IBlutilityModule* BlutilityModule = FModuleManager::GetModulePtr<IBlutilityModule>("Blutility");
	return BlutilityModule->GetAssetCategory();
}


void FAssetTypeActions_EditorUtilityBlueprint::ExecuteNewDerivedBlueprint(TWeakObjectPtr<UEditorUtilityBlueprint> InObject)
{
	if (auto Object = InObject.Get())
	{
		// The menu option should ONLY be available if there is only one blueprint selected, validated by the menu creation code
		UBlueprint* TargetBP = Object;
		UClass* TargetClass = TargetBP->GeneratedClass;

		if (!FKismetEditorUtilities::CanCreateBlueprintOfClass(TargetClass))
		{
			FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("InvalidClassToMakeBlueprintFrom", "Invalid class with which to make a Blueprint."));
			return;
		}

		FString Name;
		FString PackageName;
		CreateUniqueAssetName(Object->GetOutermost()->GetName(), TEXT("_Child"), PackageName, Name);
		const FString PackagePath = FPackageName::GetLongPackagePath(PackageName);

		UEditorUtilityBlueprintFactory* BlueprintFactory = NewObject<UEditorUtilityBlueprintFactory>();
		BlueprintFactory->ParentClass = TargetClass;

		FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
		ContentBrowserModule.Get().CreateNewAsset(Name, PackagePath, UEditorUtilityBlueprint::StaticClass(), BlueprintFactory);
	}
}

void FAssetTypeActions_EditorUtilityBlueprint::ExecuteRun(FWeakBlueprintPointerArray InObjects)
{
	UEditorUtilitySubsystem* EditorUtilitySubsystem = GEditor->GetEditorSubsystem<UEditorUtilitySubsystem>();
	for (auto ObjIt = InObjects.CreateIterator(); ObjIt; ++ObjIt)
	{
		EditorUtilitySubsystem->TryRun(ObjIt->Get());
	}
}

#undef LOCTEXT_NAMESPACE
