// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassGameplayEditorModule.h"
#include "Modules/ModuleManager.h"
//#include "Developer/AssetTools/Public/AssetToolsModule.h"
#include "MassProcessor.h"


#define LOCTEXT_NAMESPACE "MassGameplayEditor"

IMPLEMENT_MODULE(FMassGameplayEditorModule, MassGameplayEditor)

void FMassGameplayEditorModule::StartupModule()
{
	// Register the details customizers
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

	RegisterSectionMappings();
	
	PropertyModule.NotifyCustomizationModuleChanged();
}

void FMassGameplayEditorModule::ShutdownModule()
{
}

void FMassGameplayEditorModule::RegisterSectionMappings()
{
	static const FName PropertyEditor("PropertyEditor");
	FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>(PropertyEditor);

	{
		TSharedRef<FPropertySection> Section = PropertyModule.FindOrCreateSection("MassSpawner", "Mass", LOCTEXT("Mass", "Mass"));
		Section->AddCategory("Mass");
		Section->AddCategory("Debug");
	}

	{
		TSharedRef<FPropertySection> Section = PropertyModule.FindOrCreateSection("MassAgentComponent", "Mass", LOCTEXT("Mass", "Mass"));
		Section->AddCategory("Mass");
	}
}
#undef LOCTEXT_NAMESPACE
