// Copyright Epic Games, Inc. All Rights Reserved.

#include "Replication/Processing/ObjectReplicationProcessor.h"

#include "ConcertMessageData.h"
#include "Replication/Processing/IReplicationDataSource.h"

namespace UE::ConcertSyncCore
{
	FObjectReplicationProcessor::FObjectReplicationProcessor(
		TSharedRef<IReplicationDataSource> DataSource
		)
		: DataSource(MoveTemp(DataSource))
	{}

	void FObjectReplicationProcessor::ProcessObjects(const FProcessObjectsParams& Params)
	{
		// TODO UE-190714: Respect time budget and prioritize objects
		DataSource->ForEachPendingObject([this](const FConcertReplicatedObjectId& ObjectInfo)
		{
			ProcessObject({ ObjectInfo });
		});
	}
}
