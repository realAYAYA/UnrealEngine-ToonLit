// Copyright Epic Games, Inc. All Rights Reserved.

#include "ClothingSystemEditorInterfaceModule.h"

#include "ClothingAssetFactoryInterface.h"
#include "Containers/Array.h"
#include "Features/IModularFeatures.h"
#include "HAL/Platform.h"
#include "Modules/ModuleManager.h"
#include "SimulationEditorExtender.h"
#include "UObject/Class.h"

IMPLEMENT_MODULE(FClothingSystemEditorInterfaceModule, ClothingSystemEditorInterface);

const FName FClothingSystemEditorInterfaceModule::ExtenderFeatureName(TEXT("ClothingSimulationEditorExtender"));

FClothingSystemEditorInterfaceModule::FClothingSystemEditorInterfaceModule()
{

}

void FClothingSystemEditorInterfaceModule::StartupModule()
{

}

void FClothingSystemEditorInterfaceModule::ShutdownModule()
{

}

UClothingAssetFactoryBase* FClothingSystemEditorInterfaceModule::GetClothingAssetFactory()
{
	TArray<IClothingAssetFactoryProvider*> Providers = IModularFeatures::Get().GetModularFeatureImplementations<IClothingAssetFactoryProvider>(IClothingAssetFactoryProvider::FeatureName);

	// If we have providers, take the last one to register so clothing asst factories can be overridden
	if(Providers.Num() > 0)
	{
		IClothingAssetFactoryProvider* Provider = Providers.Last();
		return Provider->GetFactory();
	}

	return nullptr;
}

ISimulationEditorExtender* FClothingSystemEditorInterfaceModule::GetSimulationEditorExtender(FName InSimulationClassName)
{
	TArray<ISimulationEditorExtender*> Extenders = IModularFeatures::Get().GetModularFeatureImplementations<ISimulationEditorExtender>(ExtenderFeatureName);

	for(ISimulationEditorExtender* Extender : Extenders)
	{
		UClass* SupportedClass = Extender->GetSupportedSimulationFactoryClass();

		if(SupportedClass && SupportedClass->GetFName() == InSimulationClassName)
		{
			return Extender;
		}
	}

	return nullptr;
}
