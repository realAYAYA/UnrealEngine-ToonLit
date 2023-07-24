// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class DATASMITHGLTFTRANSLATOR_API IDatasmithGLTFTranslatorModule : public IModuleInterface
{
public:
	static const TCHAR* ModuleName;

	/**
	* Singleton-like access to the importer module
	*
	* @return Returns importer singleton instance, loading the module on demand if needed
	*/
	static inline IDatasmithGLTFTranslatorModule& Get()
	{
		return FModuleManager::LoadModuleChecked<IDatasmithGLTFTranslatorModule>(ModuleName);
	}

	/**
	* Checks to see if this module is loaded and ready. It is only valid to call Get() if IsAvailable() returns true.
	*
	* @return True if the module is loaded and ready to use
	*/
	static bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded(ModuleName);
	}
};

