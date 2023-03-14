// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

struct FGLTFImporterContext;

/**
 * The public interface of the GLTFImporter module
 */
class IGLTFImporterModule : public IModuleInterface
{
public:
	/**
	 * Singleton-like access to IGLTFImporter
	 *
	 * @return Returns IGLTFImporter singleton instance, loading the module on demand if needed
	 */
	static IGLTFImporterModule& Get()
	{
		return FModuleManager::LoadModuleChecked<IGLTFImporterModule>("GLTFImporter");
	}

	/**
	 * Access to the internal context that can be used to import GLTF files
	 *
	 * @return Returns the internal context
	 */
	virtual FGLTFImporterContext& GetImporterContext() = 0;

	/**
	 * Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("GLTFImporter");
	}
};
