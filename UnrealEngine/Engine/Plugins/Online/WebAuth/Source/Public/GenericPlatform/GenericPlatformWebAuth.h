// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

class IWebAuth;

/**
 * Platform specific WebAuth implementations
 * Intended usage is to use FPlatformWebAuth instead of FGenericPlatformWebAuth
 */
class WEBAUTH_API FGenericPlatformWebAuth
{
public:
	/**
	 * Creates a platform-specific WebAuth.
	 *
	 * @return nullptr if no implementation is available
	 */
	static IWebAuth* CreatePlatformWebAuth()
	{
		return nullptr;
	}
};

