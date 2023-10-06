// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"


/**
 * Module interface for OpenSSL cryptographic functionality. Users should generally go through the FEncryptionContext API.
 */
class IPlatformCryptoOpenSSL : public IModuleInterface
{

public:

	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static inline IPlatformCryptoOpenSSL& Get()
	{
		return FModuleManager::LoadModuleChecked< IPlatformCryptoOpenSSL >( "PlatformCryptoOpenSSL" );
	}

	/**
	 * Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded( "PlatformCryptoOpenSSL" );
	}
};


#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
