// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GenericPlatform/GenericPlatformHttp.h"


/**
 * Platform specific HTTP implementations.
 */
class HTTP_API FWindowsPlatformHttp : public FGenericPlatformHttp
{
public:

	/** Platform initialization step. */
	static void Init();

	/**
	 * Creates a platform-specific HTTP manager.
	 *
	 * @return nullptr if default implementation is to be used.
	 */
	static FHttpManager* CreatePlatformHttpManager();

	/** Platform shutdown step. */
	static void Shutdown();

	/**
	 * Creates a new HTTP request instance for the current platform.
	 *
	 * @return The request object.
	 */
	static IHttpRequest* ConstructRequest();

	/**
	 * @return the mime type for the file.
	 */
	static FString GetMimeType(const FString& FilePath);

	/**
	 * Get the proxy address specified by the operating system
	 *
	 * @return optional FString: If unset: we are unable to get information from the operating system. If set: the proxy address set by the operating system (may be blank)
	 */
	static TOptional<FString> GetOperatingSystemProxyAddress();

	/**
	 * Check if getting proxy information from the current operating system is supported
	 * Useful for "Network Settings" type pages.  GetProxyAddress may return an empty or populated string but that does not imply
	 * the operating system does or does not support proxies (or that it has been implemented here)
	 * 
	 * @return true if we are able to get proxy information from the current operating system, false if not
	 */
	static bool IsOperatingSystemProxyInformationSupported();
	
	/**
	 * Verify Peer Ssl Certificate
	 *
	 * @return optional bool: the previous value
	 */
	static bool VerifyPeerSslCertificate(bool verify);
};

#if WINDOWS_USE_FEATURE_PLATFORMHTTP_CLASS
typedef FWindowsPlatformHttp FPlatformHttp;
#endif // WINDOWS_USE_FEATURE_PLATFORMHTTP_CLASS
