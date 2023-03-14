// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

using IEOSPlatformHandlePtr = TSharedPtr<class IEOSPlatformHandle>;
class FLazySingleton;

namespace UE::Online
{

/** Factory class to create EOS Platforms for online services */
class FOnlineServicesEOSGSPlatformFactory
{
public:
	/**
	 * Get the platform factory
	 * @return the platform factory singleton
	 */
	static FOnlineServicesEOSGSPlatformFactory& Get();
	/**
	 * Tear down the singleton instance. This only cleans up the singleton and has no impact on any platform handles created by this (aside from DefaultEOSPlatformHandle's ref count decreasing).
	 */
	static void TearDown();

	/**
	 * Create a new platform. Loads configuration from ini.
	 * @return a new platform, or null on failure
	 */
	IEOSPlatformHandlePtr CreatePlatform();
	/**
	 * Get the default platform.
	 * The default platform is created on startup and can be used by the default instance of online services EOS.
	 * @return the default platform
	 */
	IEOSPlatformHandlePtr GetDefaultPlatform() { return DefaultEOSPlatformHandle; }
private:
	/** Default constructor */
	FOnlineServicesEOSGSPlatformFactory();
	friend FLazySingleton;
	/** Platform handle for the default instance */
	IEOSPlatformHandlePtr DefaultEOSPlatformHandle;
};

/* UE::Online */ }
