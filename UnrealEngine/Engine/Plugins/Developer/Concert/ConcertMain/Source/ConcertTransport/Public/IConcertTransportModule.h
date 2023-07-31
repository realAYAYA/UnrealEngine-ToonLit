// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "IConcertTransportLoggerPtr.h"

class IConcertLocalEndpoint;
struct FConcertEndpointSettings;

/**
* Interface for an Endpoint Provider
*/
class IConcertEndpointProvider
{
public:
	virtual ~IConcertEndpointProvider() {}

	/** */
	virtual TSharedPtr<IConcertLocalEndpoint> CreateLocalEndpoint(const FString& InEndpointFriendlyName, const FConcertEndpointSettings& InEndpointSettings, const FConcertTransportLoggerFactory& InLogFactory) const = 0;
};

/**
 * Interface for the Concert Transport module.
 */
class IConcertTransportModule : public IModuleInterface
{
public:
	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static inline IConcertTransportModule& Get()
	{
		static const FName ModuleName = "ConcertTransport";
		return FModuleManager::LoadModuleChecked<IConcertTransportModule>(ModuleName);
	}

	/**
	 * Checks to see if this module is loaded and ready.  It is only valid to call Get() during shutdown if IsAvailable() returns true.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static inline bool IsAvailable()
	{
		static const FName ModuleName = "ConcertTransport";
		return FModuleManager::Get().IsModuleLoaded(ModuleName);
	}

	/** Create a local transport endpoint */
	virtual TSharedPtr<IConcertEndpointProvider> CreateEndpointProvider() const = 0;
};
