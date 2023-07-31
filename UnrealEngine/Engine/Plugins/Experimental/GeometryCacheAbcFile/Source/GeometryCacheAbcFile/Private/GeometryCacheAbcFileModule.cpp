// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

#include "GeometryCacheAbcFileComponent.h"
#include "GeometryCacheAbcFileComponentCustomization.h"
#include "PropertyEditorModule.h"

class FGeometryCacheAbcFileModule : public IModuleInterface
{
	virtual void StartupModule() override
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.RegisterCustomClassLayout(UGeometryCacheAbcFileComponent::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FGeometryCacheAbcFileComponentCustomization::MakeInstance));
	}

	virtual void ShutdownModule() override
	{
		if (UObjectInitialized() && !IsEngineExitRequested())
		{
			FPropertyEditorModule* PropertyModule = FModuleManager::GetModulePtr<FPropertyEditorModule>("PropertyEditor");
			if (PropertyModule)
			{
				PropertyModule->UnregisterCustomClassLayout(UGeometryCacheAbcFileComponent::StaticClass()->GetFName());
			}
		}
	}
};

IMPLEMENT_MODULE(FGeometryCacheAbcFileModule, GeometryCacheAbcFile);