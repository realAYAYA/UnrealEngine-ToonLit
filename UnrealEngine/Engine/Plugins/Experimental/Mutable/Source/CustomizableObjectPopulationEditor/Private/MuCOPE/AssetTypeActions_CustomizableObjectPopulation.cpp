// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOPE/AssetTypeActions_CustomizableObjectPopulation.h"

#include "Delegates/Delegate.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "HAL/PlatformCrt.h"
#include "Internationalization/Internationalization.h"
#include "Modules/ModuleManager.h"
#include "MuCOE/CustomizableObjectEditorModule.h"
#include "MuCOP/CustomizableObjectPopulation.h"
#include "MuCOPE/CustomizableObjectPopulationEditorModule.h"
#include "Templates/Casts.h"
#include "Textures/SlateIcon.h"
#include "Toolkits/IToolkit.h"
#include "UObject/Object.h"

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

	//MenuBuilder.AddMenuEntry(
	//	NSLOCTEXT("AssetTypeActions_CustomizableObjectPopulation", "CustomizableObjectPopulation_CreatePopulationGenerator", "Create Population Generator"),
	//	NSLOCTEXT("AssetTypeActions_CustomizableObjectPopulation", "CustomizableObjectPopulation_CreatePopulationGeneratorToolTip", "Compiles the Population and Creates a Population Generator Asset"),
	//	FSlateIcon(),
	//	FUIAction(
	//		FExecuteAction::CreateSP(this, &FAssetTypeActions_CustomizableObjectPopulation::CreatePopulationGeneratorAsset, Objects),
	//		FCanExecuteAction()
	//	)
	//);

	//MenuBuilder.AddMenuEntry(
	//	NSLOCTEXT("AssetTypeActions_CustomizableObjectPopulation", "CustomizableObjectPopulation_RecompilePopulationGenerator", "Recompile Population Generators"),
	//	NSLOCTEXT("AssetTypeActions_CustomizableObjectPopulation", "CustomizableObjectPopulation_RecompilePopulationGeneratorToolTip", "Recompiles all the Population Generator Assets"),
	//	FSlateIcon(),
	//	FUIAction(
	//		FExecuteAction::CreateSP(this, &FAssetTypeActions_CustomizableObjectPopulation::RecompilePopulationGenerators, Objects),
	//		FCanExecuteAction()
	//	)
	//);
}

//void FAssetTypeActions_CustomizableObjectPopulation::CreatePopulationGeneratorAsset(TArray<TWeakObjectPtr<UCustomizableObjectPopulation>> Objects)
//{
//	const FString DefaultSuffix = TEXT("_Generator");
//
//	TArray<UObject*> ObjectsToSync;
//
//	for (auto ObjIt = Objects.CreateConstIterator(); ObjIt; ++ObjIt)
//	{
//		auto Object = (*ObjIt).Get();
//		if (Object)
//		{
//			if (!Object->IsValidPopulation())
//			{
//				UE_LOG(LogMutable, Warning, TEXT("There are one or more unassigned Population Classes. Please review your %s Population."), *(Object->Name));
//				continue;
//			}
//
//			// Determine an appropriate name
//			FString Name;
//			FString PackageName;
//			CreateUniqueAssetName(Object->GetOutermost()->GetName(), DefaultSuffix, PackageName, Name);
//			UPackage* Pkg = CreatePackage(*PackageName);
//
//			// Create avoid asset
//			UCustomizableObjectPopulation* CustomizableObject = Cast<UCustomizableObjectPopulation>(Object);
//			UCustomizableObjectPopulationGenerator* Generator = NewObject<UCustomizableObjectPopulationGenerator>(Pkg, FName(*Name), RF_Public | RF_Standalone);
//
//			// Fill the Generator information
//			FCustomizableObjectPopulationGeneratorCompiler GeneratorCompiler;
//			GeneratorCompiler.CompilePopulation(Object, true, Generator);
//
//			if (Generator)
//			{
//				// Mark the package dirty...
//				Pkg->MarkPackageDirty();
//
//				ObjectsToSync.Add(Generator);
//			}
//		}
//	}
//
//	if (ObjectsToSync.Num() > 0)
//	{
//		FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
//		ContentBrowserModule.Get().SyncBrowserToAssets(ObjectsToSync, /*bAllowLockedBrowsers=*/true);
//	}
//}
//
//void FAssetTypeActions_CustomizableObjectPopulation::RecompilePopulationGenerators(TArray<TWeakObjectPtr<UCustomizableObjectPopulation>> Populations)
//{
//	FCustomizableObjectPopulationGeneratorCompiler GeneratorCompiler;
//	for (int32 i = 0; i < Populations.Num(); ++i)
//	{
//		if (Populations[i].IsValid() && Populations[i]->IsValidPopulation())
//		{
//			GeneratorCompiler.CompilePopulation(Populations[i].Get(), false, Populations[i]->Generator);
//		}
//	}
//}

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
