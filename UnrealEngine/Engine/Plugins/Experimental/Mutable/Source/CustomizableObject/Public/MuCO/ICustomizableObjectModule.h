// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"


/**
 * The public interface of the CustomizableObject module
 */
class ICustomizableObjectModule : public IModuleInterface
{

public:

	/**
	 * Singleton-like access to ICustomizableObjectModule
	 *
	 * @return Returns CustomizableObjectModule singleton instance, loading the module on demand if needed
	 */
	static inline ICustomizableObjectModule& Get()
	{
		return FModuleManager::LoadModuleChecked< ICustomizableObjectModule >( "CustomizableObject" );
	}

	/**
	 * Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded( "CustomizableObject" );
	}

	// Return true if all engine patches are present
	virtual bool AreExtraBoneInfluencesEnabled() const = 0;

	// Return a string representing the plugin version.
	virtual FString GetPluginVersion() const = 0;

};

