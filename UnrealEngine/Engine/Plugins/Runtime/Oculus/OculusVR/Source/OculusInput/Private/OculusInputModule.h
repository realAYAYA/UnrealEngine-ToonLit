// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "IOculusInputModule.h"
#include "IInputDevice.h"
#include "Templates/SharedPointer.h"

#define LOCTEXT_NAMESPACE "OculusInput"


//-------------------------------------------------------------------------------------------------
// FOculusInputModule
//-------------------------------------------------------------------------------------------------

#if OCULUS_INPUT_SUPPORTED_PLATFORMS

PRAGMA_DISABLE_DEPRECATION_WARNINGS

namespace OculusInput
{
	class FOculusInput;
}

class FOculusInputModule : public IOculusInputModule
{
	TWeakPtr<OculusInput::FOculusInput> OculusInputDevice;

	// IInputDeviceModule overrides
	virtual void StartupModule() override;
	virtual TSharedPtr< class IInputDevice > CreateInputDevice( const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler ) override;

	// IOculusInputModule overrides
	virtual uint32 GetNumberOfTouchControllers() const override;
	virtual uint32 GetNumberOfHandControllers() const override;
	virtual TSharedPtr<IInputDevice> GetInputDevice() const override;
};

#else	//	OCULUS_INPUT_SUPPORTED_PLATFORMS

class FOculusInputModule : public FDefaultModuleImpl
{
	virtual uint32 GetNumberOfTouchControllers() const
	{
		return 0;
	};

	virtual uint32 GetNumberOfHandControllers() const
	{
		return 0;
	};
};

PRAGMA_ENABLE_DEPRECATION_WARNINGS

#endif	// OCULUS_INPUT_SUPPORTED_PLATFORMS


#undef LOCTEXT_NAMESPACE
