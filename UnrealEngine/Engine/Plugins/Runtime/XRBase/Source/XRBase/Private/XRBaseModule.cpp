// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

/**
 * Module for the XRBase module.
 */
class FXRBaseModule
	: public IModuleInterface
{
public:

	/** Default constructor. */
	FXRBaseModule( )
	{ }

	/** Destructor. */
	~FXRBaseModule( )
	{
	}

public:

	// IModuleInterface interface
};

IMPLEMENT_MODULE(FXRBaseModule, XRBase);

