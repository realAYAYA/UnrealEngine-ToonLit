// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Features/IModularFeature.h"
#include "Features/IModularFeatures.h" // for RegisterModularFeature()/UnregisterModularFeature()

class  UGroomAsset;
struct FHairGroupsCardsSourceDescription;
class  UHairCardGenerationSettings;

class HAIRCARDGENERATORFRAMEWORK_API IHairCardGenerator : public IModularFeature
{
public:
	static const FName ModularFeatureName; // "HairCardGenerator"

	virtual bool GenerateHairCardsForLOD(UGroomAsset* Groom, FHairGroupsCardsSourceDescription& CardsDesc) = 0;

	virtual bool IsCompatibleSettings(UHairCardGenerationSettings* OldSettings) = 0;
};

namespace HairCardGenerator_Utils
{
	FORCEINLINE void RegisterModularHairCardGenerator(IHairCardGenerator* Generator)
	{
		IModularFeatures::Get().RegisterModularFeature(IHairCardGenerator::ModularFeatureName, Generator);
	}

	FORCEINLINE void UnregisterModularHairCardGenerator(IHairCardGenerator* Generator)
	{
		IModularFeatures::Get().UnregisterModularFeature(IHairCardGenerator::ModularFeatureName, Generator);
	}
}