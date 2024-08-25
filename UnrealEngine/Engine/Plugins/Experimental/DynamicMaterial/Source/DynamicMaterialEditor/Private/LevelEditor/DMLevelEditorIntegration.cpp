// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMLevelEditorIntegration.h"
#include "DMLevelEditorIntegrationInstance.h"
#include "LevelEditor.h"
#include "Modules/ModuleManager.h"

namespace UE::DynamicMaterialEditor::Private
{
	FDelegateHandle LevelEditorCreatedHandle;

	FLevelEditorModule& GetLevelEditorModule()
	{
		return FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
	}

	FLevelEditorModule* GetLevelEditorModulePtr()
	{
		return FModuleManager::GetModulePtr<FLevelEditorModule>("LevelEditor");
	}

	FLevelEditorModule& LoadLevelEditorModuleChecked()
	{
		return FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
	}
}

void FDMLevelEditorIntegration::Initialize()
{
	using namespace UE::DynamicMaterialEditor::Private;

	LevelEditorCreatedHandle = LoadLevelEditorModuleChecked().OnLevelEditorCreated().AddLambda(
		[](TSharedPtr<ILevelEditor> InLevelEditor)
		{
			if (InLevelEditor.IsValid())
			{
				FDMLevelEditorIntegrationInstance::AddIntegration(InLevelEditor.ToSharedRef());
			}
		}
	);
}

void FDMLevelEditorIntegration::Shutdown()
{
	using namespace UE::DynamicMaterialEditor::Private;

	if (LevelEditorCreatedHandle.IsValid())
	{
		if (FLevelEditorModule* ModulePtr = GetLevelEditorModulePtr())
		{
			ModulePtr->OnLevelEditorCreated().Remove(LevelEditorCreatedHandle);
			LevelEditorCreatedHandle.Reset();
		}
	}

	FDMLevelEditorIntegrationInstance::RemoveIntegrations();
}

TSharedPtr<SDMEditor> FDMLevelEditorIntegration::GetEditorForWorld(UWorld* InWorld)
{
	// If we have an invalid world, return the first level editor integration (for assets)... if possible
	if (!IsValid(InWorld))
	{
		using namespace UE::DynamicMaterialEditor::Private;

		if (TSharedPtr<ILevelEditor> FirstLevelEditor = GetLevelEditorModule().GetFirstLevelEditor())
		{
			InWorld = FirstLevelEditor->GetWorld();
		}

		if (!IsValid(InWorld))
		{
			return nullptr;
		}
	}

	if (const FDMLevelEditorIntegrationInstance* Integration = FDMLevelEditorIntegrationInstance::GetIntegrationForWorld(InWorld))
	{
		return Integration->GetEditor();
	}

	return nullptr;
}

TSharedPtr<SDockTab> FDMLevelEditorIntegration::InvokeTabForWorld(UWorld* InWorld)
{
	// If we have an invalid world, return the first level editor integration (for assets)... if possible
	if (!IsValid(InWorld))
	{
		using namespace UE::DynamicMaterialEditor::Private;

		if (TSharedPtr<ILevelEditor> FirstLevelEditor = GetLevelEditorModule().GetFirstLevelEditor())
		{
			InWorld = FirstLevelEditor->GetWorld();
		}

		if (!IsValid(InWorld))
		{
			return nullptr;
		}
	}

	if (const FDMLevelEditorIntegrationInstance* Integration = FDMLevelEditorIntegrationInstance::GetIntegrationForWorld(InWorld))
	{
		return Integration->InvokeTab();
	}

	return nullptr;
}
