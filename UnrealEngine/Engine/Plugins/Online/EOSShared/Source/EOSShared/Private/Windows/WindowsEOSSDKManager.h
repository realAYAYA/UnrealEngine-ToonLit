// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_EOS_SDK

#include "EOSSDKManager.h"

class FWindowsEOSSDKManager : public FEOSSDKManager
{
public:
	using Super = FEOSSDKManager;

	FWindowsEOSSDKManager();
	virtual ~FWindowsEOSSDKManager();
protected:
	virtual IEOSPlatformHandlePtr CreatePlatform(const FEOSSDKPlatformConfig& PlatformConfig, EOS_Platform_Options& PlatformOptions) override;

	virtual const void* GetIntegratedPlatformOptions() override;
	virtual EOS_IntegratedPlatformType GetIntegratedPlatformType() override;


	EOS_IntegratedPlatform_Steam_Options PlatformSteamOptions;
};

using FPlatformEOSSDKManager = FWindowsEOSSDKManager;

#endif // WITH_EOS_SDK