// Copyright Epic Games, Inc. All Rights Reserved.

#include "LocalReplicationClient.h"

#include "IConcertSyncClient.h"
#include "Replication/Authority/AuthoritySynchronizer_LocalClient.h"
#include "Replication/Submission/SubmissionWorkflow_LocalClient.h"

namespace UE::MultiUserClient
{
	FLocalReplicationClient::FLocalReplicationClient(
		FReplicationDiscoveryContainer& InDiscoveryContainer,
		FGlobalAuthorityCache& InAuthorityCache,
		UMultiUserReplicationClientPreset& InSessionContent,
		TUniquePtr<IClientStreamSynchronizer> InStreamSynchronizer,
		TSharedRef<IConcertSyncClient> InClient
		)
		: FReplicationClient(
			InClient->GetConcertClient()->GetCurrentSession()->GetSessionClientEndpointId(),
			InDiscoveryContainer,
			InAuthorityCache,
			InSessionContent,
			MoveTemp(InStreamSynchronizer),
			MakeUnique<FAuthoritySynchronizer_LocalClient>(InClient),
			MakeUnique<FSubmissionWorkflow_LocalClient>(MoveTemp(InClient)))
		, RemoteSubmissionListener(InClient->GetConcertClient()->GetCurrentSession().ToSharedRef(), GetStreamSynchronizer(), GetSubmissionQueue())
	{}
}
