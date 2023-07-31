// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleManager.h"

#include "PropertyEditorModule.h"
#include "RivermaxMediaOutput.h"
#include "RivermaxMediaSource.h"
#include "Customization/RivermaxMediaDetailsCustomization.h"

class FRivermaxEditorModule : public IModuleInterface
{
public:

	//~ Begin IModuleInterface
	virtual void StartupModule() override
	{
		RegisterCustomizations();
	}
	
	virtual void ShutdownModule() override
	{
		UnregisterCustomizations();
	}
	//~End IModuleInterface

private:
	/** Registers detail customization for Rivermax media types */
	void RegisterCustomizations()
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

		const auto AddCustomization = [&PropertyModule, this](const FName ClassName, FOnGetDetailCustomizationInstance Customizor)
		{
			PropertyModule.RegisterCustomClassLayout(ClassName, Customizor);
			CustomizedClasses.Add(ClassName);
		};

		AddCustomization(URivermaxMediaSource::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FRivermaxMediaDetailsCustomization::MakeInstance));
		AddCustomization(URivermaxMediaOutput::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FRivermaxMediaDetailsCustomization::MakeInstance));
	}

	/** Unregisters registered customizations */
	void UnregisterCustomizations()
	{
		FPropertyEditorModule* PropertyModule = FModuleManager::GetModulePtr<FPropertyEditorModule>("PropertyEditor");
		if (PropertyModule)
		{
			for (FName ClassName : CustomizedClasses)
			{
				PropertyModule->UnregisterCustomClassLayout(ClassName);
			}
		}
	}

private:
	TArray<FName> CustomizedClasses;
};


IMPLEMENT_MODULE(FRivermaxEditorModule, RivermaxMediaEditor);
