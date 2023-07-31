// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "UObject/NameTypes.h"

#include "HLMediaSource.h"


/**
 * Implements the HLMediaEditor module.
 */
class FHLMediaEditorModule
	: public IModuleInterface
{
public:

	//~ IModuleInterface interface
	virtual void StartupModule() override
	{
		HLMediaSourceName = UHLMediaSource::StaticClass()->GetFName();
	}

	virtual void ShutdownModule() override
	{
	}

private:

	/** Class names. */
	FName HLMediaSourceName;
};


IMPLEMENT_MODULE(FHLMediaEditorModule, HLMediaEditor);
