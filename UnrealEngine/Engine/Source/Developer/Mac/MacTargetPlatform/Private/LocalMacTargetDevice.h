// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	LocalMacTargetDevice.h: Declares the TLocalMacTargetDevice class.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/ITargetDevice.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"

/**
 * Template for local Mac target devices.
 */
class FLocalMacTargetDevice
	: public ITargetDevice
{
public:

	/**
	 * Creates and initializes a new device for the specified target platform.
	 *
	 * @param InTargetPlatform - The target platform.
	 */
	FLocalMacTargetDevice( const ITargetPlatform& InTargetPlatform )
		: TargetPlatform(InTargetPlatform)
	{ }


public:

	virtual bool Connect( ) override
	{
		return true;
	}

	virtual void Disconnect( )
	{ }

	virtual ETargetDeviceTypes GetDeviceType( ) const override
	{
		return ETargetDeviceTypes::Desktop;
	}

	virtual FTargetDeviceId GetId( ) const override
	{
		return FTargetDeviceId(TargetPlatform.PlatformName(), GetName());
	}

	virtual FString GetName( ) const override
	{
		return FPlatformProcess::ComputerName();
	}

	virtual FString GetOperatingSystemName( ) override
	{
		return TEXT("macOS");
	}

	virtual int32 GetProcessSnapshot( TArray<FTargetDeviceProcessInfo>& OutProcessInfos ) override
	{
		// @todo Mac: implement process snapshots
		return 0;
	}

	virtual const class ITargetPlatform& GetTargetPlatform( ) const override
	{
		return TargetPlatform;
	}

	virtual bool IsConnected( )
	{
		return true;
	}

	virtual bool IsDefault( ) const override
	{
		return true;
	}

	virtual bool PowerOff( bool Force ) override
	{
		return false;
	}

	virtual bool PowerOn( ) override
	{
		return false;
	}

	virtual bool Reboot( bool bReconnect = false ) override
	{
#if PLATFORM_MAC
		NSAppleScript* Script = [[NSAppleScript alloc] initWithSource:@"tell application \"System Events\" to restart"];
		NSDictionary* ErrorDict = [NSDictionary dictionary];
		[Script executeAndReturnError: &ErrorDict];
#endif
		return true;
	}

	virtual bool SupportsFeature( ETargetDeviceFeatures Feature ) const override
	{
		switch (Feature)
		{
		case ETargetDeviceFeatures::MultiLaunch:
			return true;

		// @todo Mac: implement process snapshots
		case ETargetDeviceFeatures::ProcessSnapshot:
			return false;

		case ETargetDeviceFeatures::Reboot:
			return true;
		}

		return false;
	}

	virtual void SetUserCredentials( const FString& UserName, const FString& UserPassword ) override
	{
	}

	virtual bool GetUserCredentials( FString& OutUserName, FString& OutUserPassword ) override
	{
		return false;
	}

	virtual bool TerminateProcess( const int64 ProcessId ) override
	{
		return false;
	}


private:

	// Holds a reference to the device's target platform.
	const ITargetPlatform& TargetPlatform;
};
