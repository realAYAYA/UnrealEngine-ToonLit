// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GenericPlatform/GenericApplicationMessageHandler.h"
#include "Features/IModularFeatures.h"
#include "Features/IModularFeature.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "IInputDevice.h"

/** Input parameters to device creation. */
struct INPUTDEVICE_API FInputDeviceCreationParameters
{
	/** Indicates if this device is operating as a primary device and thus a part of game input system. */
	bool bInitAsPrimaryDevice = true;
};

/**
 * The public interface of the InputDeviceModule
 */
class IInputDeviceModule : public IModuleInterface, public IModularFeature
{
public:
	static FName GetModularFeatureName()
	{
		static FName FeatureName = FName(TEXT("InputDevice"));
		return FeatureName;
	}

	virtual void StartupModule() override
	{
		IModularFeatures::Get().RegisterModularFeature( GetModularFeatureName(), this );
	}

	/**
	 * Singleton-like access to IInputDeviceModule
	 *
	 * @return Returns IInputDeviceModule singleton instance, loading the module on demand if needed
	 */
	static inline IInputDeviceModule& Get()
	{
		return FModuleManager::LoadModuleChecked< IInputDeviceModule >( "InputDevice" );
	}

	/**
	 * Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded( "InputDevice" );
	}

	/**
	 * Attempts to create a new input device with the given message handler.  This version will eventually be deprecated and users should
	 * migrate to the new version with an additional parameters struct.
	 *
	 * @return	Interface to the new input device, if we were able to successfully create one
	 */
	virtual TSharedPtr<class IInputDevice> CreateInputDevice(const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler) = 0;

	/**
	 * Attempts to create a new input device interface with the specified input
	 * device creation parameters. This override is for advanced use cases where
	 * users would like to create devices that are not a part of the input
	 * system.
	 *
	 * @return	Interface to the new input device, if we were able to successfully create one
	 */
	virtual TSharedPtr<class IInputDevice> CreateInputDevice(const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler, FInputDeviceCreationParameters InParams)
	{
		// If this is a primary device and it should be constructed for the input system then call the default implementation.
		if (InParams.bInitAsPrimaryDevice)
		{
			return CreateInputDevice(InMessageHandler);
		}

		return nullptr;
	}
};
