// Copyright Epic Games, Inc. All Rights Reserved.

#include "CEEditorModule.h"

#include "Cloner/CEClonerActor.h"
#include "Cloner/CEEditorClonerDetailCustomization.h"
#include "Effector/CEEditorEffectorDetailCustomization.h"
#include "Effector/CEEffectorActor.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "Styles/CEEditorStyle.h"

void FCEEditorModule::StartupModule()
{
	FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>(PropertyEditorName);

	// Load styles
	FCEEditorStyle::Get();

	// Cloner/effector customization
	PropertyModule.RegisterCustomClassLayout(ACEEffectorActor::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FCEEditorEffectorDetailCustomization::MakeInstance));
	PropertyModule.RegisterCustomClassLayout(ACEClonerActor::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FCEEditorClonerDetailCustomization::MakeInstance));
}

void FCEEditorModule::ShutdownModule()
{
	if (FModuleManager::Get().IsModuleLoaded(PropertyEditorName))
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>(PropertyEditorName);

		// Cloner/effector
		PropertyModule.UnregisterCustomClassLayout(ACEEffectorActor::StaticClass()->GetFName());
		PropertyModule.UnregisterCustomClassLayout(ACEClonerActor::StaticClass()->GetFName());
	}
}

IMPLEMENT_MODULE(FCEEditorModule, ClonerEffectorEditor)