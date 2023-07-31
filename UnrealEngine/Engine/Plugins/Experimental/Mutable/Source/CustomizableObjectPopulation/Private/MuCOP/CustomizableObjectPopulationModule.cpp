// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "HAL/Platform.h"
#include "HAL/PlatformCrt.h"
#include "Interfaces/IPluginManager.h"
#include "Modules/ModuleManager.h"
#include "MuCO/CustomizableObject.h"
#include "MuCOE/ICustomizableObjectPopulationModule.h"
#include "MuCOP/CustomizableObjectPopulation.h"
#include "MuCOP/CustomizableObjectPopulationGenerator.h"
#include "PluginDescriptor.h"
#include "Templates/Casts.h"
#include "Templates/SharedPointer.h"
#include "UObject/Class.h"
#include "UObject/NameTypes.h"
#include "UObject/Object.h"
#include "UObject/TopLevelAssetPath.h"
#include "UObject/UObjectGlobals.h"


/**
 * MovieSceneCore module implementation (private)
 */
class FCustomizableObjectPopulationModule : public ICustomizableObjectPopulationModule
{

public:

	// IModuleInterface 
	void StartupModule() override{}
	void ShutdownModule() override{}

	// ICustomizableObjectPopulationModule interface
	FString GetPluginVersion() const override;

#if WITH_EDITOR
	// Recompiles all the populations referenced in the Customizabled Object
	virtual void RecompilePopulations(UCustomizableObject* Object);
#endif

};


IMPLEMENT_MODULE( FCustomizableObjectPopulationModule, CustomizableObjectPopulation );


//-------------------------------------------------------------------------------------------------

FString FCustomizableObjectPopulationModule::GetPluginVersion() const
{
	FString Version = "x.x";
	TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin("MutablePopulation");
	if (Plugin.IsValid() && Plugin->IsEnabled())
	{
		Version = Plugin->GetDescriptor().VersionName;
	}
	return Version;
}


#if WITH_EDITOR
void FCustomizableObjectPopulationModule::RecompilePopulations(UCustomizableObject* Object)
{
	if (!Object)
	{
		return;
	}

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	
	// Getting all the referencers of the CO
	TArray<FName> COAssetNames;
	AssetRegistryModule.Get().GetReferencers(*Object->GetOuter()->GetPathName(), COAssetNames);
	
	if (COAssetNames.Num())
	{
		// Creating a filter to search only population classes
		FARFilter COFilter;
		COFilter.ClassPaths.Add(UCustomizableObjectPopulation::StaticClass()->GetClassPathName());

		for (const FName AssetName : COAssetNames)
		{
			COFilter.PackageNames.Add(AssetName);
		}

		//Getting population class assets
		TArray<FAssetData> PopulationAssetData;
		AssetRegistryModule.Get().GetAssets(COFilter, PopulationAssetData);

		for (int32 i = 0; i < PopulationAssetData.Num(); ++i)
		{
			if (UCustomizableObjectPopulation* Population = Cast<UCustomizableObjectPopulation>(PopulationAssetData[i].GetAsset()))
			{
				if (Population->IsValidPopulation())
				{
					//for (int32 j = 0; j < Population->ClassWeights.Num(); ++j)
					//{
					//	//Population->ClassWeights[j].Class->CustomizableObject->RefreshVersionId();
					//	Population->ClassWeights[j].Class->CustomizableObject->LoadDerivedData(true);
					//}
					Population->CompilePopulation(NewObject<UCustomizableObjectPopulationGenerator>());
				}
			}
		}
	}
}
#endif // WITH_EDITOR
