// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Replication/Editor/Model/Extension/IStreamExtender.h"
#include "Replication/Editor/Model/Extension/StreamExtenderBySettings.h"

#include "Misc/Guid.h"

namespace UE::MultiUserClient
{
	class FReplicationDiscoveryContainer;
	
	/**
	 * When the user adds an object, this object handles auto selecting properties and adding additional objects from context.
	 * The following sources exist:
	 * - Static Settings: user can specify properties & objects in the MU project settings
	 * - Dynamic API: by using the IMultiUserReplication, external modules can register dynamic rules for auto-discovery using IReplicationDiscoverer.
	 *
	 * Every FReplicationClient owns an instance of this and executes it when an object is added for the client by the local editor.
	 */
	class FMultiUserStreamExtender : public ConcertSharedSlate::IStreamExtender
	{
	public:
		
		/**
		 * @param InClientId The ID of the owning FReplicationClient instance.
		 * @param InRegisteredExtenders Holds all IReplicationDiscoverer that were registered to Multi User. The caller ensures it outlives the constructed instance.
		 */
		FMultiUserStreamExtender(const FGuid& InClientId, FReplicationDiscoveryContainer& InRegisteredExtenders);

		//~ Begin IStreamExtender Interface
		virtual void ExtendStream(UObject& ExtendedObject, ConcertSharedSlate::IStreamExtensionContext& Context) override;
		//~ End IStreamExtender Interface

	private:

		/** The client ID to which this extender is adds objects. */
		const FGuid ClientId;

		/** Handles properties from the MU settings. */
		ConcertClientSharedSlate::FStreamExtenderBySettings ExtendBySettings;

		/** Allows generic extenders to extend added objects. These can be added via the IMultiUserReplication interface. */
		FReplicationDiscoveryContainer& RegisteredExtenders;
		
		void ExtendStreamWithRegisteredDiscoverers(UObject& ExtendedObject, ConcertSharedSlate::IStreamExtensionContext& Context) const;
	};
}

