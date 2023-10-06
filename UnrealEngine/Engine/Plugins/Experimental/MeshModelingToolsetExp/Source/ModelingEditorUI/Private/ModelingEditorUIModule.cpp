// Copyright Epic Games, Inc. All Rights Reserved.

#include "ModelingEditorUIModule.h"

#include "SkinWeightDetailCustomization.h"
#include "SkeletalMesh/SkinWeightsPaintTool.h"
#include "PropertyEditorModule.h"

#define LOCTEXT_NAMESPACE "FModelingEditorUIModule"

void FModelingEditorUIModule::StartupModule()
{
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	
	auto RegisterDetailCustomization = [&](FName InStructName, auto InCustomizationFactory)
    {
    	PropertyEditorModule.RegisterCustomClassLayout(
    		InStructName,
    		FOnGetDetailCustomizationInstance::CreateStatic(InCustomizationFactory)
    	);
    	CustomizedClasses.Add(InStructName);
    };

	// register detail customizations
	RegisterDetailCustomization(USkinWeightsPaintToolProperties::StaticClass()->GetFName(), &FSkinWeightDetailCustomization::MakeInstance);
	
	PropertyEditorModule.NotifyCustomizationModuleChanged();
}

void FModelingEditorUIModule::ShutdownModule()
{
	if (FPropertyEditorModule* PropertyModule = FModuleManager::GetModulePtr<FPropertyEditorModule>("PropertyEditor"))
	{
		for (const FName& ClassName : CustomizedClasses)
		{
			PropertyModule->UnregisterCustomClassLayout(ClassName);
		}
		PropertyModule->NotifyCustomizationModuleChanged();
	}
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FModelingEditorUIModule, ModelingEditorUI)