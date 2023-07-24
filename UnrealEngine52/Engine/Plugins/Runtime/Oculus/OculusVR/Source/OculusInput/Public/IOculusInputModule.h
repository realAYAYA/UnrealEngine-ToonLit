// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Modules/ModuleManager.h"
#include "IInputDeviceModule.h"

#define OCULUS_INPUT_SUPPORTED_PLATFORMS (PLATFORM_WINDOWS || PLATFORM_ANDROID_ARM64)


/**
 * The public interface to this module.  In most cases, this interface is only public to sibling modules 
 * within this plugin.
 */
class UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.") IOculusInputModule;
class IOculusInputModule : public IInputDeviceModule
{

public:

	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static inline IOculusInputModule& Get()
	{
		return FModuleManager::LoadModuleChecked< IOculusInputModule >( "OculusInput" );
	}

	/**
	 * Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded( "OculusInput" );
	}

	/**
	 * Gets the number of Touch controllers that are active, so that games that require them can check to make sure they're present
	 *
	 * @return The number of Touch controllers that are active (but not necessarily tracked)
	 */
	virtual uint32 GetNumberOfTouchControllers() const = 0;

	/**
	 * Gets the number of hands that are active, so that games that require them can check to make sure they're present
	 *
	 * @return The number of Hands that are active (but not necessarily tracked)
	 */
	virtual uint32 GetNumberOfHandControllers() const = 0;

	virtual TSharedPtr<IInputDevice> GetInputDevice() const = 0;
};

