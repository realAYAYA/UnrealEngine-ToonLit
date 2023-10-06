// Copyright Epic Games, Inc. All Rights Reserved.

#include "PropertyEditorModule.h"
#include "XRDeviceVisualizationDetails.h"
#include "Modules/ModuleManager.h"

/**
 * Module for the XRCore editor module.
 */
class FXRBaseEditorModule
	: public IModuleInterface
{
public:

	/** Default constructor. */
	FXRBaseEditorModule( )
	{ }

	/** Destructor. */
	~FXRBaseEditorModule( )
	{
	}

public:

	// IModuleInterface interface

	virtual void StartupModule() override
	{
		// register settings detail panel customization
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.RegisterCustomClassLayout("XRDeviceVisualizationComponent", FOnGetDetailCustomizationInstance::CreateStatic(&FXRDeviceVisualizationDetails::MakeInstance));
	}
};

IMPLEMENT_MODULE(FXRBaseEditorModule, XRBaseEditor);

