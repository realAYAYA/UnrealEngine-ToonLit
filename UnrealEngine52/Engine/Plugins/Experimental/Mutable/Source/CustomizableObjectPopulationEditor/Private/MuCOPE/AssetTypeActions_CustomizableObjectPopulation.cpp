// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOPE/AssetTypeActions_CustomizableObjectPopulation.h"

#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "MuCOE/CustomizableObjectEditorModule.h"
#include "MuCOP/CustomizableObjectPopulation.h"
#include "MuCOPE/CustomizableObjectPopulationEditorModule.h"

class IToolkitHost;
class UClass;

#define LOCTEXT_NAMESPACE "AssetTypeActions_CustomizableObjectPopulation"


FText FAssetTypeActions_CustomizableObjectPopulation::GetName() const
{
	return LOCTEXT("AssetTypeActions_CustomizableObjectPopulation", "Customizable Population");
}

FColor FAssetTypeActions_CustomizableObjectPopulation::GetTypeColor() const
{ 
	return FColor(0, 0, 0); 
}

UClass* FAssetTypeActions_CustomizableObjectPopulation::GetSupportedClass() const
{ 
	return UCustomizableObjectPopulation::StaticClass(); 
}

uint32 FAssetTypeActions_CustomizableObjectPopulation::GetCategories()
{ 
	const ICustomizableObjectEditorModule* CustomizableObjectEditorModule = &FModuleManager::LoadModuleChecked<ICustomizableObjectEditorModule>("CustomizableObjectEditor");
	return CustomizableObjectEditorModule->GetAssetCategory();
}

void FAssetTypeActions_CustomizableObjectPopulation::GetActions(const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder)
{
	TArray<TWeakObjectPtr<UCustomizableObjectPopulation>> Objects = GetTypedWeakObjectPtrs<UCustomizableObjectPopulation>(InObjects);

	MenuBuilder.AddMenuEntry(
		NSLOCTEXT("AssetTypeActions_CustomizableObjectPopulation", "CustomizableObjectPopulation_RecompilePopulations", "Recompile Populations"),
		NSLOCTEXT("AssetTypeActions_CustomizableObjectPopulation", "CustomizableObjectPopulation_RecompilePopulationsToolTip", "Recompiles all the Mutable Populations Assets"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP(this, &FAssetTypeActions_CustomizableObjectPopulation::RecompilePopulations, Objects),
			FCanExecuteAction()
		)
	);
}

void FAssetTypeActions_CustomizableObjectPopulation::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor)
{
	EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		UCustomizableObjectPopulation* Object = Cast<UCustomizableObjectPopulation>(*ObjIt);
		if (Object != NULL)
		{
			ICustomizableObjectPopulationEditorModule* CustomizableObjectPopulationEditorModule = &FModuleManager::LoadModuleChecked<ICustomizableObjectPopulationEditorModule>("CustomizableObjectPopulationEditor");
			CustomizableObjectPopulationEditorModule->CreateCustomizableObjectPopulationEditor(Mode, EditWithinLevelEditor, Object);
		}
	}
}

void FAssetTypeActions_CustomizableObjectPopulation::RecompilePopulations(TArray<TWeakObjectPtr<UCustomizableObjectPopulation>> Populations)
{
	for (int32 i = 0; i < Populations.Num(); ++i)
	{
		if (Populations[i].IsValid() && Populations[i]->IsValidPopulation())
		{
			Populations[i].Get()->CompilePopulation();
		}
	}
}

#undef LOCTEXT_NAMESPACE
