// Copyright Epic Games, Inc. All Rights Reserved.

#include "ClothingSimulationFactory.h"
#include "HAL/IConsoleManager.h"
#include "Features/IModularFeatures.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ClothingSimulationFactory)

const FName IClothingSimulationFactoryClassProvider::FeatureName = TEXT("ClothingSimulationFactoryClassProvider");

namespace ClothingSimulationFactoryConsoleVariables
{
	TAutoConsoleVariable<FString> CVarDefaultClothingSimulationFactoryClass(
		TEXT("p.Cloth.DefaultClothingSimulationFactoryClass"),
		TEXT("ChaosClothingSimulationFactory"),  // Chaos is the default provider when Chaos Cloth is enabled
		TEXT("The class name of the default clothing simulation factory.\n")
		TEXT("Known providers are:\n")
		TEXT("ChaosClothingSimulationFactory\n")
		, ECVF_Cheat);
}

TSubclassOf<class UClothingSimulationFactory> UClothingSimulationFactory::GetDefaultClothingSimulationFactoryClass()
{
	TSubclassOf<UClothingSimulationFactory> DefaultClothingSimulationFactoryClass = nullptr;

	const FString DefaultClothingSimulationFactoryClassName = ClothingSimulationFactoryConsoleVariables::CVarDefaultClothingSimulationFactoryClass.GetValueOnAnyThread();
	
	IModularFeatures::Get().LockModularFeatureList();
	const TArray<IClothingSimulationFactoryClassProvider*> ClassProviders = IModularFeatures::Get().GetModularFeatureImplementations<IClothingSimulationFactoryClassProvider>(IClothingSimulationFactoryClassProvider::FeatureName);
	IModularFeatures::Get().UnlockModularFeatureList();
	for (const auto& ClassProvider : ClassProviders)
	{
		if (ClassProvider)
		{
			const TSubclassOf<UClothingSimulationFactory> ClothingSimulationFactoryClass = ClassProvider->GetClothingSimulationFactoryClass();
			if (ClothingSimulationFactoryClass.Get() != nullptr)
			{
				// Always set the default to the last non null factory class (in case the search for the cvar doesn't yield any results)
				DefaultClothingSimulationFactoryClass = ClothingSimulationFactoryClass;

				// Early exit if the cvar string matches
				if (ClothingSimulationFactoryClass->GetName() == DefaultClothingSimulationFactoryClassName)
				{
					break;
				}
			}
		}
	}

	return DefaultClothingSimulationFactoryClass;
}

