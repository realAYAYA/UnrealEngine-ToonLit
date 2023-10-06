// Copyright Epic Games, Inc. All Rights Reserved.

#include "PhysicsControlEditorModule.h"
#include "PhysicsControlComponent.h"
#include "PhysicsControlComponentVisualizer.h"
#include "PhysicsControlProfileAssetActions.h"
#include "PhysicsControlProfileEditorMode.h"
#include "UnrealEdGlobals.h"
#include "Editor/UnrealEdEngine.h"

#define LOCTEXT_NAMESPACE "PhysicsControlModule"

//#define ENABLE_PHYSICS_CONTROL_PROFILE_EDITOR

//======================================================================================================================
void FPhysicsControlEditorModule::StartupModule()
{
	// Physics Control Profile editor/asset is disabled for now
#ifdef ENABLE_PHYSICS_CONTROL_PROFILE_EDITOR
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	PhysicsControlProfileAssetActions = MakeShared<FPhysicsControlProfileAssetActions>();
	AssetTools.RegisterAssetTypeActions(PhysicsControlProfileAssetActions.ToSharedRef());

	FEditorModeRegistry::Get().RegisterMode<FPhysicsControlProfileEditorMode>(
		FPhysicsControlProfileEditorMode::ModeName, 
		LOCTEXT("PhysicsControlProfileEditorMode", "PhysicsControlProfile"), 
		FSlateIcon(), false);
#endif

	if (GUnrealEd)
	{
		TSharedPtr<FPhysicsControlComponentVisualizer> Visualizer = MakeShared<FPhysicsControlComponentVisualizer>();
		GUnrealEd->RegisterComponentVisualizer(UPhysicsControlComponent::StaticClass()->GetFName(), Visualizer);
		// This call should maybe be inside the RegisterComponentVisualizer call above, but since it's not,
		// we'll put it here.
		Visualizer->OnRegister();
		VisualizersToUnregisterOnShutdown.Add(UPhysicsControlComponent::StaticClass()->GetFName());
	}
}

//======================================================================================================================
void FPhysicsControlEditorModule::ShutdownModule()
{
	// Physics Control Profile editor/asset is disabled for now
#ifdef ENABLE_PHYSICS_CONTROL_PROFILE_EDITOR
	FEditorModeRegistry::Get().UnregisterMode(FPhysicsControlProfileEditorMode::ModeName);

	if (FModuleManager::Get().IsModuleLoaded("AssetTools"))
	{
		FAssetToolsModule::GetModule().Get().UnregisterAssetTypeActions(
			PhysicsControlProfileAssetActions.ToSharedRef());
	}
#endif

	if (GUnrealEd)
	{
		for (const FName& Name : VisualizersToUnregisterOnShutdown)
		{
			GUnrealEd->UnregisterComponentVisualizer(Name);
		}
	}
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FPhysicsControlEditorModule, PhysicsControlEditor)


