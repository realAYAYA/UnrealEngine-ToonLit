// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMLevelEditorIntegrationInstance.h"
#include "Components/PrimitiveComponent.h"
#include "DMWorldSubsystem.h"
#include "DynamicMaterialEditorModule.h"
#include "EditorModeManager.h"
#include "Elements/Framework/TypedElementSelectionSet.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "ILevelEditor.h"
#include "Material/DynamicMaterialInstance.h"
#include "Materials/Material.h"
#include "Model/DynamicMaterialModel.h"
#include "Selection.h"
#include "Slate/SDMEditor.h"
#include "Styling/SlateIconFinder.h"
#include "Widgets/Docking/SDockTab.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

#define LOCTEXT_NAMESPACE "DMLevelEditorIntegration"

TArray<FDMLevelEditorIntegrationInstance, TInlineAllocator<1>> FDMLevelEditorIntegrationInstance::Instances;

const FDMLevelEditorIntegrationInstance* FDMLevelEditorIntegrationInstance::AddIntegration(const TSharedRef<ILevelEditor>& InLevelEditor)
{
	ValidateInstances();

	for (const FDMLevelEditorIntegrationInstance& Instance : Instances)
	{
		if (Instance.LevelEditorWeak == InLevelEditor)
		{
			return &Instance;
		}
	}

	// Create a new instance directly in the Instances array.
	new(Instances) FDMLevelEditorIntegrationInstance(InLevelEditor);

	return &Instances.Last();
}

void FDMLevelEditorIntegrationInstance::RemoveIntegrations()
{
	Instances.Empty();
}

const FDMLevelEditorIntegrationInstance* FDMLevelEditorIntegrationInstance::GetIntegrationForWorld(UWorld* InWorld)
{
	if (!IsValid(InWorld))
	{
		return nullptr;
	}

	ValidateInstances();

	for (const FDMLevelEditorIntegrationInstance& Instance : Instances)
	{
		// Always return the first level editor integration for null words - they are assets.
		if (!InWorld)
		{
			return &Instance;
		}

		if (TSharedPtr<ILevelEditor> LevelEditor = Instance.LevelEditorWeak.Pin())
		{
			if (LevelEditor->GetWorld() == InWorld)
			{
				return &Instance;
			}
		}
	}

	return nullptr;
}

FDMLevelEditorIntegrationInstance::~FDMLevelEditorIntegrationInstance()
{
	UnregisterSelectionChange();
	UnregisterWithTabManager();
}

const TSharedPtr<SDMEditor>& FDMLevelEditorIntegrationInstance::GetEditor() const
{
	return Editor;
}

TSharedPtr<SDockTab> FDMLevelEditorIntegrationInstance::InvokeTab() const
{
	TSharedPtr<ILevelEditor> LevelEditor = LevelEditorWeak.Pin();

	if (!LevelEditor.IsValid())
	{
		return nullptr;
	}

	TSharedPtr<FTabManager> TabManager = LevelEditor->GetTabManager();

	if (!TabManager.IsValid())
	{
		return nullptr;
	}

	TSharedPtr<SDockTab> Tab = TabManager->FindExistingLiveTab(FDynamicMaterialEditorModule::TabId);

	if (!Tab.IsValid())
	{
		Tab = TabManager->TryInvokeTab(FDynamicMaterialEditorModule::TabId);
	}

	if (Tab.IsValid())
	{
		Tab->ActivateInParent(ETabActivationCause::SetDirectly);
		Tab->DrawAttention();
	}

	return Tab;
}

void FDMLevelEditorIntegrationInstance::FDMLevelEditorIntegrationInstance::ValidateInstances()
{
	for (int32 Index = 0; Index < Instances.Num(); ++Index)
	{
		if (!Instances[Index].LevelEditorWeak.IsValid())
		{
			Instances.RemoveAt(Index);
			--Index;
		}
	}
}

FDMLevelEditorIntegrationInstance::FDMLevelEditorIntegrationInstance(const TSharedRef<ILevelEditor>& InLevelEditor)
{
	LevelEditorWeak = InLevelEditor;

	Editor = StaticCastSharedRef<SDMEditor>(
		FDynamicMaterialEditorModule::CreateEditor(nullptr, nullptr)
	);

	RegisterSelectionChange();
	RegisterWithTabManager();
}

void FDMLevelEditorIntegrationInstance::RegisterSelectionChange()
{
	TSharedPtr<ILevelEditor> LevelEditor = LevelEditorWeak.Pin();

	if (!LevelEditor.IsValid())
	{
		return;
	}

	if (USelection* Selection = LevelEditor->GetEditorModeManager().GetSelectedActors())
	{
		if (UTypedElementSelectionSet* ActorSelectionSet = Selection->GetElementSelectionSet())
		{
			ActorSelectionSet->OnChanged().AddRaw(this, &FDMLevelEditorIntegrationInstance::OnActorSelectionChanged);
			ActorSelectionSetWeak = ActorSelectionSet;
		}
	}

	if (USelection* Selection = LevelEditor->GetEditorModeManager().GetSelectedObjects())
	{
		if (UTypedElementSelectionSet* ObjectSelectionSet = Selection->GetElementSelectionSet())
		{
			ObjectSelectionSet->OnChanged().AddRaw(this, &FDMLevelEditorIntegrationInstance::OnObjectSelectionChanged);
			ObjectSelectionSetWeak = ObjectSelectionSet;
		}
	}
}

void FDMLevelEditorIntegrationInstance::UnregisterSelectionChange()
{
	UTypedElementSelectionSet* ActorSelectionSet = ActorSelectionSetWeak.Get();

	if (ActorSelectionSet)
	{
		ActorSelectionSet->OnChanged().RemoveAll(this);
	}

	UTypedElementSelectionSet* ObjectSelectionSet = ObjectSelectionSetWeak.Get();

	if (ObjectSelectionSet)
	{
		ObjectSelectionSet->OnChanged().RemoveAll(this);
	}
}

void FDMLevelEditorIntegrationInstance::RegisterWithTabManager()
{
	TSharedPtr<ILevelEditor> LevelEditor = LevelEditorWeak.Pin();

	if (!LevelEditor.IsValid())
	{
		return;
	}

	TSharedPtr<FTabManager> TabManager = LevelEditor->GetTabManager();

	if (!TabManager.IsValid())
	{
		return;
	}

	static const FText TabText = LOCTEXT("MaterialDesignerTabName", "Material Designer");

	TabManager->RegisterTabSpawner(
		FDynamicMaterialEditorModule::TabId,
		FOnSpawnTab::CreateLambda(
			[this](const FSpawnTabArgs&)
			{
				return SNew(SDockTab)
					.TabRole(ETabRole::PanelTab)
					.Content()
					[
						Editor.ToSharedRef()
					];
			}
		)
	)
	.SetDisplayNameAttribute(TabText)
	.SetDisplayName(TabText)
	.SetGroup(WorkspaceMenu::GetMenuStructure().GetLevelEditorCategory())
	.SetIcon(FSlateIconFinder::FindIconForClass(UMaterial::StaticClass()));
}

void FDMLevelEditorIntegrationInstance::UnregisterWithTabManager()
{
	TSharedPtr<ILevelEditor> LevelEditor = LevelEditorWeak.Pin();

	if (!LevelEditor.IsValid())
	{
		return;
	}

	TSharedPtr<FTabManager> TabManager = LevelEditor->GetTabManager();

	if (!TabManager.IsValid())
	{
		return;
	}

	TabManager->UnregisterTabSpawner(FDynamicMaterialEditorModule::TabId);
}

void FDMLevelEditorIntegrationInstance::OnActorSelectionChanged(const UTypedElementSelectionSet* InSelectionSet)
{
	AActor* NewSelectedActor = nullptr;

	for (AActor* Actor : InSelectionSet->GetSelectedObjects<AActor>())
	{
		// Only do this if we have a single selected actor.
		if (NewSelectedActor != nullptr)
		{
			return;
		}

		NewSelectedActor = Actor;
	}

	if (NewSelectedActor)
	{
		OnActorSelected(NewSelectedActor);
	}
}

void FDMLevelEditorIntegrationInstance::OnActorSelected(AActor* InActor)
{
	if (!IsValid(InActor))
	{
		return;
	}

	UDynamicMaterialInstance* Instance = nullptr;

	InActor->ForEachComponent<UPrimitiveComponent>(false, [&Instance](const UPrimitiveComponent* InPrimComp)
		{
			// Can't break this, so just skip every component
			if (Instance)
			{
				return;
			}

			for (int32 MaterialIdx = 0; MaterialIdx < InPrimComp->GetNumMaterials(); ++MaterialIdx)
			{
				if (UDynamicMaterialInstance* MDI = Cast<UDynamicMaterialInstance>(InPrimComp->GetMaterial(MaterialIdx)))
				{
					Instance = MDI;
					return;
				}
			}
		});

	UDynamicMaterialModel* MaterialModel = Instance ? Instance->GetMaterialModel() : nullptr;

	if (!MaterialModel)
	{
		Editor->SetMaterialActor(InActor);
	}
	else
	{
		OnMaterialModelSelected(MaterialModel);
	}
}

void FDMLevelEditorIntegrationInstance::OnObjectSelectionChanged(const UTypedElementSelectionSet* InSelectionSet)
{
	UDynamicMaterialModel* NewSelectedMaterialModel = nullptr;

	for (UDynamicMaterialModel* MaterialModel : InSelectionSet->GetSelectedObjects<UDynamicMaterialModel>())
	{
		// Only do this if we have a single selected instance.
		if (NewSelectedMaterialModel != nullptr)
		{
			return;
		}

		NewSelectedMaterialModel = MaterialModel;
	}

	if (NewSelectedMaterialModel)
	{
		OnMaterialModelSelected(NewSelectedMaterialModel);
		return;
	}

	UDynamicMaterialInstance* NewSelectedMaterialInstance = nullptr;

	for (UDynamicMaterialInstance* MaterialInstance : InSelectionSet->GetSelectedObjects<UDynamicMaterialInstance>())
	{
		// Only do this if we have a single selected model.
		if (NewSelectedMaterialInstance != nullptr)
		{
			return;
		}

		NewSelectedMaterialInstance = MaterialInstance;
	}

	if (NewSelectedMaterialInstance)
	{
		OnMaterialModelSelected(NewSelectedMaterialInstance->GetMaterialModel());
		return;
	}
}

void FDMLevelEditorIntegrationInstance::OnMaterialModelSelected(UDynamicMaterialModel* InMaterialModel)
{
	if (InMaterialModel)
	{
		Editor->SetMaterialModel(InMaterialModel);
	}
}

#undef LOCTEXT_NAMESPACE
