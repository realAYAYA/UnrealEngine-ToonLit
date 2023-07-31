// Copyright Epic Games, Inc. All Rights Reserved.

#include "PosixShimModule.h"
#include "PosixShim.h"

class FPosixShimModule : public IPosixShimModule
{
private:
	// IModuleInterface Interface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

IMPLEMENT_MODULE( FPosixShimModule, PosixShim );

void FPosixShimModule::StartupModule()
{
}

void FPosixShimModule::ShutdownModule()
{
}
