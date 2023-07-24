// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "GenericPlatform/GenericPlatformHttp.h"

/**
 * Platform specific Http implementations
 */
class HTTP_API FApplePlatformHttp : public FGenericPlatformHttp
{
public:

	/**
	 * Platform initialization step
	 */
	static void Init();

	/**
	 * Creates a platform-specific HTTP manager.
	 *
	 * @return NULL if default implementation is to be used
	 */
	static FHttpManager* CreatePlatformHttpManager()
	{
		return nullptr;
	}

	/**
	 * Platform shutdown step
	 */
	static void Shutdown();

	/**
	 * Creates a new Http request instance for the current platform
	 *
	 * @return request object
	 */
	static IHttpRequest* ConstructRequest();

private:
	/** Flag to allow fall back to use NSUrlConnection instead of NSUrlSession. Assigned from commandline */
	static inline bool bUseNSUrlSession = false;

    /** Session used to create Apple based requests */
    static inline NSURLSession* Session = nil;

	static void InitWithNSUrlSession();
	static void ShutdownWithNSUrlSession();
};


typedef FApplePlatformHttp FPlatformHttp;
