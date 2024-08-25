// Copyright Epic Games, Inc. All Rights Reserved.

#include "RemoteReplicationClient.h"

#include "IConcertClient.h"
#include "Assets/MultiUserReplicationClientPreset.h"
#include "Replication/Authority/AuthoritySynchronizer_RemoteClient.h"
#include "Replication/Stream/StreamSynchronizer_RemoteClient.h"
#include "Replication/Submission/Remote/SubmissionWorkflow_RemoteClient.h"

namespace UE::MultiUserClient
{
	FRemoteReplicationClient::FRemoteReplicationClient(
		const FGuid& InConcertClientId,
		FReplicationDiscoveryContainer& InDiscoveryContainer,
		TSharedRef<IConcertClient> InClient,
		FGlobalAuthorityCache& InAuthorityCache,
		UMultiUserReplicationClientPreset& InSessionContent,
		FRegularQueryService& QueryService
		)
		: FReplicationClient(
			InConcertClientId,
			InDiscoveryContainer,
			InAuthorityCache,
			InSessionContent,
			MakeUnique<FStreamSynchronizer_RemoteClient>(InConcertClientId, QueryService),
			MakeUnique<FAuthoritySynchronizer_RemoteClient>(InConcertClientId,QueryService),
			MakeUnique<FSubmissionWorkflow_RemoteClient>(InClient->GetCurrentSession().ToSharedRef(), InConcertClientId))
	{}
}
