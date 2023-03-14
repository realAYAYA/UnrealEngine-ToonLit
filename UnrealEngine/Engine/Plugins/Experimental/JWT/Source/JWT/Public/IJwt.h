// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "JwtIncludes.h"


/**
 * The public interface to this module.  In most cases, this interface is only public to sibling modules 
 * within this plugin.
 */
class IJwt : public IModuleInterface
{

public:
	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static inline IJwt& Get()
	{
		IJwt* const ModulePtr = FModuleManager::GetModulePtr<IJwt>("JWT");
		if (ModulePtr)
		{
			return *ModulePtr;
		}

		return FModuleManager::LoadModuleChecked<IJwt>("JWT");
	}

	/**
	 * Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("JWT");
	}
};

namespace UE::JWT
{
	JWT_API TOptional<FJsonWebToken> FromString(const FStringView InEncodedJsonWebToken, const bool bIsSignatureEncoded);

	JWT_API bool FromString(const FStringView InEncodedJWT, FJsonWebToken& OutJsonWebToken, const bool bIsSignatureEncoded);
}
