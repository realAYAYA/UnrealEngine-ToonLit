// Copyright Epic Games, Inc. All Rights Reserved.

#include "PIEPreviewDeviceSpecification.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

class FPIEPreviewDeviceSpecificationModule
	: public IModuleInterface
{
public:

	// IModuleInterface interface

	virtual void StartupModule() override { }
	virtual void ShutdownModule() override { }
};


IMPLEMENT_MODULE(FPIEPreviewDeviceSpecificationModule, PIEPreviewDeviceSpecification);

UPIEPreviewDeviceSpecification::UPIEPreviewDeviceSpecification(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}
