// Copyright Epic Games, Inc. All Rights Reserved.

#include "PropertyAnimatorCoreEditorModule.h"

#include "Animators/PropertyAnimatorCoreBase.h"
#include "Customizations/PropertyAnimatorCoreEditorDetailCustomization.h"
#include "Customizations/PropertyAnimatorCoreEditorContextTypeCustomization.h"
#include "Modules/ModuleManager.h"
#include "Properties/PropertyAnimatorCoreContext.h"
#include "PropertyEditorModule.h"

void FPropertyAnimatorCoreEditorModule::StartupModule()
{
	FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomPropertyTypeLayout(UPropertyAnimatorCoreContext::StaticClass()->GetFName(), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FPropertyAnimatorCoreEditorContextTypeCustomization::MakeInstance));
	PropertyModule.RegisterCustomClassLayout(UPropertyAnimatorCoreBase::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FPropertyAnimatorCoreEditorDetailCustomization::MakeInstance));
}

void FPropertyAnimatorCoreEditorModule::ShutdownModule()
{
	if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.UnregisterCustomPropertyTypeLayout(UPropertyAnimatorCoreContext::StaticClass()->GetFName());
		PropertyModule.UnregisterCustomClassLayout(UPropertyAnimatorCoreBase::StaticClass()->GetFName());
	}
}

IMPLEMENT_MODULE(FPropertyAnimatorCoreEditorModule, PropertyAnimatorCoreEditor)
