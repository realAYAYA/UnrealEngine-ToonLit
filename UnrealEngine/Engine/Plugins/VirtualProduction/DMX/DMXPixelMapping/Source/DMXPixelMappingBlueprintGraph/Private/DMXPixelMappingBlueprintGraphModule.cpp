// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

#include "CoreMinimal.h"

#include "DMXPixelMappingPinFactory.h"

class FDMXPixelMappingBlueprintGraphModule 
	: public IModuleInterface
{
public:
	//~ Begin IModuleInterface implementation
	virtual void StartupModule() override
	{
		PixelMappingPinFactory = MakeShared<FDMXPixelMappingPinFactory>();
		FEdGraphUtilities::RegisterVisualPinFactory(PixelMappingPinFactory);
	}

	virtual void ShutdownModule() override
	{
		FEdGraphUtilities::UnregisterVisualPinFactory(PixelMappingPinFactory);
	}
	//~ End IModuleInterface implementation

private:
	TSharedPtr<FDMXPixelMappingPinFactory> PixelMappingPinFactory;
};

IMPLEMENT_MODULE(FDMXPixelMappingBlueprintGraphModule, DMXPixelMappingBlueprintGraph);
