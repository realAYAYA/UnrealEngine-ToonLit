// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IClientAuthoritySynchronizer.h"

#include "Containers/Set.h"
#include "UObject/SoftObjectPath.h"

struct FGuid;
struct FConcertAuthorityClientInfo;

namespace UE::MultiUserClient
{
	class FRegularQueryService;

	class FAuthoritySynchronizer_RemoteClient : public FAuthoritySynchronizer_Base
	{
	public:
		
		FAuthoritySynchronizer_RemoteClient(const FGuid& RemoteEndpointId, FRegularQueryService& InQueryService);
		virtual ~FAuthoritySynchronizer_RemoteClient();

		//~ Begin IClientAuthoritySynchronizer Interface
		virtual bool HasAnyAuthority() const override;
		virtual bool HasAuthorityOver(const FSoftObjectPath& ObjectPath) const override { return LastServerState.Contains(ObjectPath); }
		//~ End IClientAuthoritySynchronizer Interface

	private:

		/** Queries the server in regular intervals. This services outlives our object. */
		FRegularQueryService& QueryService;
		
		/** Used to unregister HandleStreamQuery upon destruction. */
		const FDelegateHandle QueryStreamHandle;

		/** The most up to date server state of the remote client's authority. */
		TSet<FSoftObjectPath> LastServerState; 

		void HandleAuthorityQuery(const TArray<FConcertAuthorityClientInfo>& PerStreamAuthority);
	};
}


