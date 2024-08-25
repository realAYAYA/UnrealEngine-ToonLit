// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "IHairCardGenerator.h"

class IHairCardGeneratorEditor : public IModuleInterface, public IHairCardGenerator
{
public:
	static FORCEINLINE IHairCardGeneratorEditor& Get()
	{
		return FModuleManager::LoadModuleChecked<IHairCardGeneratorEditor>("HairCardGeneratorEditor");
	}
};
