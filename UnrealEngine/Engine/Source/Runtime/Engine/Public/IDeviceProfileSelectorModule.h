// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	IDeviceProfileSelectorModule.h: Declares the IDeviceProfileSelectorModule interface.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"


/**
 * Device Profile Selector module
 */
class IDeviceProfileSelectorModule
	: public IModuleInterface
{
public:

	/**
	 * Run the logic to choose an appropriate device profile for this session
	 *
	 * @return The name of the device profile to use for this session
	 */
	virtual const FString GetRuntimeDeviceProfileName() = 0;

	/**
	* Run the logic to choose an appropriate device profile for this session.
	* @param DeviceParameters	A map of parameters to be used by device profile logic.
	* @return The name of the device profile to use for this session
	*/
	virtual const FString GetDeviceProfileName() { return FString(); }

	/**
	* Set or override the selector specific properties.
	* @param SelectorProperties	A map of parameters to be used by device profile matching logic.
	*/
	virtual void SetSelectorProperties(const TMap<FName, FString>& SelectorProperties) { }

	/*
	* Find a custom profile selector property value.
	* @Param PropertyType The information requested
	* @Param PropertyValueOUT the value of PropertyType
	* @return Whether the PropertyType was recognized by the profile selector.
	*/
	virtual bool GetSelectorPropertyValue(const FName& PropertyType, FString& PropertyValueOUT) { PropertyValueOUT.Reset(); return false; }

	/**
	 * Virtual destructor.
	 */
	virtual ~IDeviceProfileSelectorModule()
	{
	}
};
