// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IRivermaxCoreModule.h"


namespace UE::RivermaxCore
{
	class IRivermaxManager;
}

/**
 * Implementation of the IRivermaxCoreModule managing Rivermax library and stream creation
 */
class FRivermaxCoreModule : public IRivermaxCoreModule
{

public:
	//~ Begin IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ End IModuleInterface

	//~ Begin IRivermaxCoreModule interface
	virtual TUniquePtr<UE::RivermaxCore::IRivermaxInputStream> CreateInputStream() override;
	virtual TUniquePtr<UE::RivermaxCore::IRivermaxOutputStream> CreateOutputStream() override;
	virtual TSharedPtr<UE::RivermaxCore::IRivermaxManager> GetRivermaxManager() override;
	//~ End IRivermaxCoreModule interface

private:

	TSharedPtr<UE::RivermaxCore::IRivermaxManager> RivermaxManager;
};

