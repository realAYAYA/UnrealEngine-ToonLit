// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuCOPE/AssetTypeActions_CustomizableObjectPopulationClass.h"

#include "HAL/PlatformMath.h"
#include "Internationalization/Internationalization.h"
#include "Modules/ModuleManager.h"
#include "MuCOE/CustomizableObjectEditorModule.h"
#include "MuCOP/CustomizableObjectPopulationCharacteristic.h"
#include "MuCOP/CustomizableObjectPopulationClass.h"
#include "MuCOPE/CustomizableObjectPopulationEditorModule.h"
#include "Templates/Casts.h"
#include "Toolkits/IToolkit.h"
#include "UObject/Object.h"

class IToolkitHost;
class UClass;

#define LOCTEXT_NAMESPACE "AssetTypeActions_CustomizableObjectPopulationClass"


FText FAssetTypeActions_CustomizableObjectPopulationClass::GetName() const 
{ 
	return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_CustomizableObjectPopulationClass", "Customizable Population Class"); 
}


FColor FAssetTypeActions_CustomizableObjectPopulationClass::GetTypeColor() const 
{ 
	return FColor(255, 255, 255);
}


UClass* FAssetTypeActions_CustomizableObjectPopulationClass::GetSupportedClass() const 
{ 
	return UCustomizableObjectPopulationClass::StaticClass();
}


uint32 FAssetTypeActions_CustomizableObjectPopulationClass::GetCategories()
{ 
	const ICustomizableObjectEditorModule* CustomizableObjectEditorModule = &FModuleManager::LoadModuleChecked<ICustomizableObjectEditorModule>("CustomizableObjectEditor");
	return CustomizableObjectEditorModule->GetAssetCategory();}


void FAssetTypeActions_CustomizableObjectPopulationClass::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor)
{
	EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		UCustomizableObjectPopulationClass* Object = Cast<UCustomizableObjectPopulationClass>(*ObjIt);
		if (Object != NULL)
		{
			ICustomizableObjectPopulationEditorModule* CustomizableObjectPopulationEditorModule = &FModuleManager::LoadModuleChecked<ICustomizableObjectPopulationEditorModule>("CustomizableObjectPopulationEditor");
			CustomizableObjectPopulationEditorModule->CreateCustomizableObjectPopulationClassEditor(Mode, EditWithinLevelEditor, Object);
		}
	}
}


#undef LOCTEXT_NAMESPACE
