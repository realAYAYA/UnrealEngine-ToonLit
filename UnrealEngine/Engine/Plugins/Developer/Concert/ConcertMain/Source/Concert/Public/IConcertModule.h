// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

class IConcertServer;
class IConcertClient;
class IConcertServerEventSink;

struct FConcertSessionFilter;

typedef TSharedPtr<IConcertServer, ESPMode::ThreadSafe> IConcertServerPtr;
typedef TSharedRef<IConcertServer, ESPMode::ThreadSafe> IConcertServerRef;
typedef TSharedPtr<IConcertClient, ESPMode::ThreadSafe> IConcertClientPtr;
typedef TSharedRef<IConcertClient, ESPMode::ThreadSafe> IConcertClientRef;

/**
 * Interface for the Main Concert module.
 */
class IConcertModule : public IModuleInterface
{
public:
	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static inline IConcertModule& Get()
	{
		static const FName ModuleName = "Concert";
		return FModuleManager::LoadModuleChecked<IConcertModule>(ModuleName);
	}

	/**
	 * Checks to see if this module is loaded and ready.  It is only valid to call Get() during shutdown if IsAvailable() returns true.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static inline bool IsAvailable()
	{
		static const FName ModuleName = "Concert";
		return FModuleManager::Get().IsModuleLoaded(ModuleName);
	}

	/**
	 * Create a server that will perform a certain role (eg, MultiUser, DisasterRecovery, etc)
	 * @param InRole The role to create
	 * @param InAutoArchiveSessionFilter The session filter to apply when auto-archiving sessions
	 * @param InEventSink Sink functions for events that the server can emit
	 * @return The server
	 */
	virtual IConcertServerRef CreateServer(const FString& InRole, const FConcertSessionFilter& InAutoArchiveSessionFilter, IConcertServerEventSink* InEventSink) = 0;

	/**
	 * Create a client that will perform a certain role (eg, MultiUser, DisasterRecovery, etc)
	 * @param InRole The role to create
	 * @return The client
	 */
	virtual IConcertClientRef CreateClient(const FString& InRole) = 0;
};
