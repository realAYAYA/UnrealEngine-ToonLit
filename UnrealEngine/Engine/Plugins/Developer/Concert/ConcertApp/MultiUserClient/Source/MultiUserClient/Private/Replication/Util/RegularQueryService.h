// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Replication/Messages/ClientQuery.h"
#include "Containers/Ticker.h"
#include "Replication/IToken.h"
#include "Templates/SharedPointer.h"
#include "Templates/UnrealTemplate.h"

class IConcertSyncClient;

namespace UE::MultiUserClient
{
	DECLARE_DELEGATE_OneParam(FStreamQueryDelegate, const TArray<FConcertBaseStreamInfo>&);
	DECLARE_DELEGATE_OneParam(FAuthorityQueryDelegate, const TArray<FConcertAuthorityClientInfo>&);
	
	/** Sends regular FConcertReplication_QueryReplicationInfo_Request to endpoints and publishes the results. */
	class FRegularQueryService : public FNoncopyable
	{
	public:

		FRegularQueryService(
			TSharedRef<IConcertSyncClient> InOwningClient,
			float InInterval = 1.f
			);
		~FRegularQueryService();

		/** Registers a delegate to invoke for querying an endpoint about its registered streams. */
		FDelegateHandle RegisterStreamQuery(const FGuid& EndpointId, FStreamQueryDelegate Delegate);
		/** Registers a delegate to invoke for querying an endpoint about its authority. */
		FDelegateHandle RegisterAuthorityQuery(const FGuid& EndpointId, FAuthorityQueryDelegate Delegate);

		void UnregisterStreamQuery(const FDelegateHandle& Handle);
		void UnregisterAuthorityQuery(const FDelegateHandle& Handle);
		
	private:

		/** Used to check whether we were destroyed after a response is received. */
		const TSharedRef<FToken> Token = FToken::Make();
		
		/**
		 * Used to send queries.
		 * 
		 * This FRegularQueryService's owner is supposed to make sure this FRegularQueryService is destroyed
		 * when the client shuts down.
		 */
		const TWeakPtr<IConcertSyncClient> OwningClient;

		/** Used to unregister the ticker. */
		const FTSTicker::FDelegateHandle TickerDelegateHandle;

		// We use a multicast delegate here because it handles unsubscribing while being Broadcast
		DECLARE_MULTICAST_DELEGATE_OneParam(FMulticastStreamQueryDelegate, const TArray<FConcertBaseStreamInfo>&);
		DECLARE_MULTICAST_DELEGATE_OneParam(FMulticastAuthorityQueryDelegate, const TArray<FConcertAuthorityClientInfo>&);
		struct FStreamQueryInfo
		{
			TSet<FDelegateHandle> Handles;
			FMulticastStreamQueryDelegate Delegate;
		};
		struct FAuthorityQueryInfo
		{
			TSet<FDelegateHandle> Handles;
			FMulticastAuthorityQueryDelegate Delegate;
		};
		TMap<FGuid, FStreamQueryInfo> StreamQueryInfos;
		TMap<FGuid, FAuthorityQueryInfo> AuthorityQueryInfos;

		bool bIsHandlingQueryResponse = false;

		/** Queries the server for the client's current state. */
		void SendQueryEvent();
		void BuildStreamRequest(FConcertReplication_QueryReplicationInfo_Request& Request) const;
		void BuildAuthorityRequest(FConcertReplication_QueryReplicationInfo_Request& Request) const;
		
		/** Handles the server's response. */
		void HandleQueryResponse(const FConcertReplication_QueryReplicationInfo_Response& Response);
		void CompactDelegates();
	};
}

