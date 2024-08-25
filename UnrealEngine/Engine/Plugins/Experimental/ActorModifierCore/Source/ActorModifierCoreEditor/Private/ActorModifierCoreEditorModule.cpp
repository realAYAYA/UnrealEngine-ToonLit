// Copyright Epic Games, Inc. All Rights Reserved.

#include "ActorModifierCoreEditorModule.h"

#include "Modifiers/ActorModifierCoreStack.h"
#include "Modifiers/Customizations/ActorModifierCoreEditorDetailCustomization.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"

void FActorModifierCoreEditorModule::StartupModule()
{
	RegisterCustomizations();
}

void FActorModifierCoreEditorModule::ShutdownModule()
{
	UnregisterCustomizations();
}

void FActorModifierCoreEditorModule::RegisterCustomizations() const
{
	// Register custom layouts
	static FName PropertyEditor("PropertyEditor");
	FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>(PropertyEditor);

	PropertyModule.RegisterCustomClassLayout(UActorModifierCoreStack::StaticClass()->GetFName()
		, FOnGetDetailCustomizationInstance::CreateStatic(&FActorModifierCoreEditorDetailCustomization::MakeInstance));
}

void FActorModifierCoreEditorModule::UnregisterCustomizations() const
{
	// Unregister custom layouts
	static FName PropertyEditor("PropertyEditor");
	if (FModuleManager::Get().IsModuleLoaded(PropertyEditor))
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>(PropertyEditor);
		PropertyModule.UnregisterCustomClassLayout(UActorModifierCoreStack::StaticClass()->GetFName());
	}
}

IMPLEMENT_MODULE(FActorModifierCoreEditorModule, ActorModifierCoreEditor)
