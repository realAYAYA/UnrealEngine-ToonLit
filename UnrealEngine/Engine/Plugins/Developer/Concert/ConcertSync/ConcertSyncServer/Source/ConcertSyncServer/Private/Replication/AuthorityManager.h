// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ConcertMessages.h"
#include "Misc/EBreakBehavior.h"
#include "Replication/Data/ObjectIds.h"
#include "Templates/Function.h"

class IConcertSession;

struct FConcertReplication_ChangeAuthority_Response;
struct FConcertReplication_ChangeAuthority_Request;
struct FConcertPropertyChain;
struct FConcertPropertySelection;
struct FConcertSessionContext;
struct FConcertObjectInStreamID;
struct FConcertReplicatedObjectId;
struct FConcertReplicationStream;

namespace UE::ConcertSyncServer::Replication
{
	/**
	 * Callbacks FAuthorityManager requires to function correctly.
	 * Exists to make FAuthorityManager independent from all the other systems and allows mocking in tests.
	 */
	class IAuthorityManagerGetters
	{
	public:

		/** Provides a way to extract all streams registered to a given client. */
		virtual void ForEachStream(const FGuid& ClientEndpointId, TFunctionRef<EBreakBehavior(const FConcertReplicationStream& Stream)> Callback) const = 0;

		/** Iterates through all clients have registered to send any data. */
		virtual void ForEachSendingClient(TFunctionRef<EBreakBehavior(const FGuid& ClientEndpointId)> Callback) const = 0;

		virtual ~IAuthorityManagerGetters() = default;
	};
	
	/** Responds to FConcertChangeAuthority_Request and tracks what objects and properties clients have authority over. */
	class FAuthorityManager : public FNoncopyable
	{
	public:
		
		using FStreamId = FGuid;
		using FClientId = FGuid;
		using FProcessAuthorityConflict = TFunctionRef<EBreakBehavior(const FClientId& ClientId, const FStreamId& StreamId, const FConcertPropertyChain& WrittenProperties)>;

		FAuthorityManager(IAuthorityManagerGetters& Getters, TSharedRef<IConcertSession> InSession);
		~FAuthorityManager();

		/**
		 * Checks whether the client that sent the identified object had authority to send it.
		 * @return Whether the server should process the object change.
		 */
		bool HasAuthorityToChange(const FConcertReplicatedObjectId& ObjectChange) const;

		/** Utility for iterating authority a client has for a given stream. */
		void EnumerateAuthority(const FClientId& ClientId, const FStreamId& StreamId, TFunctionRef<EBreakBehavior(const FSoftObjectPath& Object)> Callback) const;

		enum class EAuthorityResult : uint8
		{
			/** The client is allowed to take authority */
			Allowed,
			/** There was at least one conflict */
			Conflict,
			/** No conflicts were checked because OverwriteProperties was nullptr and the client had not registered any properties to send for the given object. */
			NoRegisteredProperties
		};
		/**
		 * Enumerates all authority conflicts, if any, that would occur if ObjectChange.SenderEndpointId were to take authority over the identified object.
		 * You can optionally supply OverwriteProperties, which is useful if you're about to change the contents of the stream.
		 * @return Whether there were any conflicts
		 */
		EAuthorityResult EnumerateAuthorityConflicts(
			const FConcertReplicatedObjectId& Object,
			const FConcertPropertySelection* OverwriteProperties = nullptr,
			FProcessAuthorityConflict ProcessConflict = [](auto&, auto&, auto&){ return EBreakBehavior::Break; }
			) const;
		/** Whether it is legal for this the client identified by ClientId to take control over the object given the stream the client has registered. */
		bool CanTakeAuthority(const FConcertReplicatedObjectId& Object) const;

		/** Notifies this manager that the client has left, which means all their authority is now gone. */
		void OnClientLeft(const FClientId& ClientEndpointId);
		/** Takes away authority from the given client from the given object. */
		void RemoveAuthority(const FConcertReplicatedObjectId& Object);

	private:

		/** Callbacks required to obtain client info. */
		IAuthorityManagerGetters& Getters;
		/** The session under which this manager operates. */
		TSharedRef<IConcertSession> Session;

		struct FClientAuthorityData
		{
			/** Objects the client has authority over. */
			TMap<FStreamId, TSet<FSoftObjectPath>> OwnedObjects;
		};
		TMap<FClientId, FClientAuthorityData> ClientAuthorityData;

		EConcertSessionResponseCode HandleChangeAuthorityRequest(
			const FConcertSessionContext& ConcertSessionContext,
			const FConcertReplication_ChangeAuthority_Request& Request,
			FConcertReplication_ChangeAuthority_Response& Response
			);

		/** Finds a stream registered with the client by its ID. */
		const FConcertReplicationStream* FindClientStreamById(const FClientId& ClientId, const FStreamId& StreamId) const;
	};
}

