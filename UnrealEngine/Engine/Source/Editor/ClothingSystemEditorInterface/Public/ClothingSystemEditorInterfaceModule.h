// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"
#include "UObject/NameTypes.h"

class ISimulationEditorExtender;
class UClothingAssetFactoryBase;

class CLOTHINGSYSTEMEDITORINTERFACE_API FClothingSystemEditorInterfaceModule : public IModuleInterface
{

public:

	const static FName ExtenderFeatureName;

	FClothingSystemEditorInterfaceModule();

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	UClothingAssetFactoryBase* GetClothingAssetFactory();
	ISimulationEditorExtender* GetSimulationEditorExtender(FName InSimulationFactoryClassName);

private:

};
