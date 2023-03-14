// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"

#include "IRivermaxInputStream.h"
#include "IRivermaxOutputStream.h"
#include "Modules/ModuleManager.h"


namespace UE::RivermaxCore
{
	class IRivermaxManager;
}


/**
 * Core module for Rivermax access from the engine. Users can create different stream types that are exposed to 
 * get data flow ongoing.
 */
class IRivermaxCoreModule : public IModuleInterface
{
public:
	static inline IRivermaxCoreModule& Get()
	{
		return FModuleManager::LoadModuleChecked<IRivermaxCoreModule>("RivermaxCore");
	}

	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("RivermaxCore");
	}

	/** Create input stream managing receiving data from rivermax */
	virtual TUniquePtr<UE::RivermaxCore::IRivermaxInputStream> CreateInputStream() = 0;

	/** Create output stream managing sending data to rivermax */
	virtual TUniquePtr<UE::RivermaxCore::IRivermaxOutputStream> CreateOutputStream() = 0;

	/** Returns Rivermax manager singleton to query for stats, status, etc... */
	virtual TSharedPtr<UE::RivermaxCore::IRivermaxManager> GetRivermaxManager() = 0;
};

