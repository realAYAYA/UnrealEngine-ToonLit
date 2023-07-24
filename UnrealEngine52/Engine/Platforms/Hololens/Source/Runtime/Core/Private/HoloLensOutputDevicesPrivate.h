// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	HoloLensOutputDevicesPrivate.h: HoloLens implementations of output devices
=============================================================================*/

#pragma once
#include "Misc/OutputDeviceError.h"
#include "Misc/OutputDeviceConsole.h"

DECLARE_LOG_CATEGORY_EXTERN(LogHoloLensOutputDevices, Log, All);

/**
* Output device that writes to HoloLens Event Log
*/
class FOutputDeviceEventLog :
	public FOutputDevice
{
	Windows::Foundation::Diagnostics::LoggingChannel^ EtwLogChannel;

public:
	/**
	* Constructor, initializing member variables
	*/
	FOutputDeviceEventLog()
	{
#if !NO_LOGGING
		// Always use the default logging channel GUID here.  This is simpler than the prior approach of sometimes
		// using the session id, and UAT can still filter on the provider name.
		static const Platform::Guid MicrosoftWindowsDiagnoticsLoggingChannelId(0x4bd2826e, 0x54a1, 0x4ba9, 0xbf, 0x63, 0x92, 0xb7, 0x3e, 0xa1, 0xac, 0x4a);
		EtwLogChannel = ref new Windows::Foundation::Diagnostics::LoggingChannel(ref new Platform::String(FApp::GetProjectName()), nullptr, MicrosoftWindowsDiagnoticsLoggingChannelId);
#endif
	}

	/** Destructor that cleans up any remaining resources */
	virtual ~FOutputDeviceEventLog()
	{
		TearDown();
	}

	virtual void Serialize(const TCHAR* Buffer, ELogVerbosity::Type Verbosity, const class FName& Category) override
	{
#if !NO_LOGGING
		if (EtwLogChannel != nullptr)
		{
			Windows::Foundation::Diagnostics::LoggingLevel LogWithLevel = GetWindowsLoggingLevelFromUEVerbosity(Verbosity);
			// we need to use Information level as minimal for getting all information to the editor
			// and giving user ability to read the whole meaningful device's log
			if (LogWithLevel >= Windows::Foundation::Diagnostics::LoggingLevel::Information)
			{
				EtwLogChannel->LogMessage(ref new Platform::String(Buffer), LogWithLevel);
			}
		}
#endif
	}

	Windows::Foundation::Diagnostics::LoggingLevel GetWindowsLoggingLevelFromUEVerbosity(ELogVerbosity::Type Verbosity)
	{
		switch (Verbosity)
		{
		case ELogVerbosity::Error:
			return Windows::Foundation::Diagnostics::LoggingLevel::Error;

		case ELogVerbosity::Warning:
			return Windows::Foundation::Diagnostics::LoggingLevel::Warning;

		case ELogVerbosity::Fatal:
			return Windows::Foundation::Diagnostics::LoggingLevel::Critical;

		case ELogVerbosity::Display:
		case ELogVerbosity::Log:
			return Windows::Foundation::Diagnostics::LoggingLevel::Information;

		case ELogVerbosity::Verbose:
		case ELogVerbosity::VeryVerbose:
		default:
			return Windows::Foundation::Diagnostics::LoggingLevel::Verbose;
		}
	}

	/** Does nothing */
	virtual void Flush(void)
	{
	}

	/**
	* Closes any event log handles that are open
	*/
	virtual void TearDown(void)
	{
	}
};