// Copyright Epic Games, Inc. All Rights Reserved.

#include "EditorUtilityWidgetBlueprint.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "CoreGlobals.h"
#include "Delegates/Delegate.h"
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "EditorUtilityWidget.h"
#include "Engine/Blueprint.h"
#include "Engine/Engine.h"
#include "Framework/Docking/TabManager.h"
#include "IBlutilityModule.h"
#include "LevelEditor.h"
#include "Modules/ModuleManager.h"
#include "SlotBase.h"
#include "Templates/Casts.h"
#include "Templates/ChooseClass.h"
#include "Templates/SubclassOf.h"
#include "Types/SlateEnums.h"
#include "UObject/Package.h"
#include "UObject/UnrealNames.h"
#include "UnrealEdMisc.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"

class SWidget;
class UClass;
class UWorld;




/////////////////////////////////////////////////////
// UEditorUtilityWidgetBlueprint

UEditorUtilityWidgetBlueprint::UEditorUtilityWidgetBlueprint(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

void UEditorUtilityWidgetBlueprint::BeginDestroy()
{
	// Only cleanup script if it has been registered and we're not shutdowning editor
	if (!IsEngineExitRequested() && RegistrationName != NAME_None)
	{
		IBlutilityModule* BlutilityModule = FModuleManager::GetModulePtr<IBlutilityModule>("Blutility");
		if (BlutilityModule)
		{
			BlutilityModule->RemoveLoadedScriptUI(this);
		}

		FLevelEditorModule* LevelEditorModule = FModuleManager::GetModulePtr<FLevelEditorModule>(TEXT("LevelEditor"));
		if (LevelEditorModule)
		{
			TSharedPtr<FTabManager> LevelEditorTabManager = LevelEditorModule->GetLevelEditorTabManager();
			if (LevelEditorTabManager.IsValid())
			{
				LevelEditorTabManager->UnregisterTabSpawner(RegistrationName);
			}
		}
	}

	Super::BeginDestroy();
}


TSharedRef<SDockTab> UEditorUtilityWidgetBlueprint::SpawnEditorUITab(const FSpawnTabArgs& SpawnTabArgs)
{
	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab);

	TSharedRef<SWidget> TabWidget = CreateUtilityWidget();
	SpawnedTab->SetContent(TabWidget);
	
	SpawnedTab->SetOnTabClosed(SDockTab::FOnTabClosedCallback::CreateUObject(this, &UEditorUtilityWidgetBlueprint::UpdateRespawnListIfNeeded));
	CreatedTab = SpawnedTab;
	
	OnCompiled().AddUObject(this, &UEditorUtilityWidgetBlueprint::RegenerateCreatedTab);
	
	FLevelEditorModule& LevelEditor = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
	LevelEditor.OnMapChanged().AddUObject(this, &UEditorUtilityWidgetBlueprint::ChangeTabWorld);

	return SpawnedTab;
}

TSharedRef<SWidget> UEditorUtilityWidgetBlueprint::CreateUtilityWidget()
{
	TSharedRef<SWidget> TabWidget = SNullWidget::NullWidget;

	UClass* BlueprintClass = GeneratedClass;
	TSubclassOf<UEditorUtilityWidget> WidgetClass = BlueprintClass;
	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (World)
	{
		if (CreatedUMGWidget)
		{
			CreatedUMGWidget->Rename(nullptr, GetTransientPackage(), REN_DoNotDirty);
		}
		CreatedUMGWidget = CreateWidget<UEditorUtilityWidget>(World, WidgetClass);
		if (CreatedUMGWidget)
		{
			// Editor Utility is flagged as transient to prevent from dirty the World it's created in when a property added to the Utility Widget is changed
			CreatedUMGWidget->SetFlags(RF_Transient);

			// Also mark nested utility widgets as transient to prevent them from dirtying the world (since they'll be created via CreateWidget and not CreateUtilityWidget)
			TArray<UWidget*> AllWidgets;
			CreatedUMGWidget->WidgetTree->GetAllWidgets(AllWidgets);

			for (UWidget* Widget : AllWidgets)
			{
				if (Widget->IsA(UEditorUtilityWidget::StaticClass()))
				{
					Widget->SetFlags(RF_Transient);
					Widget->Slot->SetFlags(RF_Transient);
				}
			}
		}
	}

	if (CreatedUMGWidget)
	{
		TabWidget = SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.HAlign(HAlign_Fill)
			[
				CreatedUMGWidget->TakeWidget()
			];
	}
	return TabWidget;
}

void UEditorUtilityWidgetBlueprint::RegenerateCreatedTab(UBlueprint* RecompiledBlueprint)
{
	if (CreatedTab.IsValid())
	{
		TSharedRef<SWidget> TabWidget = CreateUtilityWidget();
		CreatedTab.Pin()->SetContent(TabWidget);
	}
}

void UEditorUtilityWidgetBlueprint::ChangeTabWorld(UWorld* World, EMapChangeType MapChangeType)
{
	if (MapChangeType == EMapChangeType::TearDownWorld)
	{
		// We need to Delete the UMG widget if we are tearing down the World it was built with.
		if (CreatedUMGWidget && World == CreatedUMGWidget->GetWorld())
		{
			if (CreatedTab.IsValid())
			{
				CreatedTab.Pin()->SetContent(SNullWidget::NullWidget);
			}
			
			CreatedUMGWidget->Rename(nullptr, GetTransientPackage());
			CreatedUMGWidget = nullptr;			
		}
	}
	else if (MapChangeType != EMapChangeType::SaveMap)
	{
		// Recreate the widget if we are loading a map or opening a new map
		RegenerateCreatedTab(nullptr);
	}
}

void UEditorUtilityWidgetBlueprint::UpdateRespawnListIfNeeded(TSharedRef<SDockTab> TabBeingClosed)
{
	if (GeneratedClass)
	{
		const UEditorUtilityWidget* EditorUtilityWidget = GeneratedClass->GetDefaultObject<UEditorUtilityWidget>();
		if (EditorUtilityWidget && EditorUtilityWidget->ShouldAlwaysReregisterWithWindowsMenu() == false)
		{
			IBlutilityModule* BlutilityModule = FModuleManager::GetModulePtr<IBlutilityModule>("Blutility");
			BlutilityModule->RemoveLoadedScriptUI(this);
		}
	}
	CreatedUMGWidget = nullptr;
}

void UEditorUtilityWidgetBlueprint::GetReparentingRules(TSet< const UClass* >& AllowedChildrenOfClasses, TSet< const UClass* >& DisallowedChildrenOfClasses) const
{
	AllowedChildrenOfClasses.Empty();
	AllowedChildrenOfClasses.Add(UEditorUtilityWidget::StaticClass());
}


