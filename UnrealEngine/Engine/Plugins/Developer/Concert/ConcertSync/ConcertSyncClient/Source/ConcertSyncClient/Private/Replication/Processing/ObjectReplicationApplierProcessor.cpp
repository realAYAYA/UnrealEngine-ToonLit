// Copyright Epic Games, Inc. All Rights Reserved.

#include "ObjectReplicationApplierProcessor.h"

#include "ConcertLogGlobal.h"
#include "Replication/IConcertClientReplicationBridge.h"
#include "Replication/Formats/IObjectReplicationFormat.h"

#include "Components/SceneComponent.h"

namespace UE::ConcertSyncClient::Replication
{
	FObjectReplicationApplierProcessor::FObjectReplicationApplierProcessor(
		IConcertClientReplicationBridge* ReplicationBridge,
		TSharedRef<ConcertSyncCore::IObjectReplicationFormat> ReplicationFormat,
		TSharedRef<ConcertSyncCore::IReplicationDataSource> DataSource
		)
		: FObjectReplicationProcessor(MoveTemp(DataSource))
		, ReplicationBridge(ReplicationBridge)
		, ReplicationFormat(MoveTemp(ReplicationFormat))
	{}

	void FObjectReplicationApplierProcessor::ProcessObject(const FObjectProcessArgs& Args)
	{
		UObject* Object = ReplicationBridge->FindObjectIfAvailable(Args.ObjectInfo.Object);
		if (!Object)
		{
			UE_LOG(LogConcert, Error, TEXT("Replication: Object %s is unavailable. The data source should not have reported it."), *Args.ObjectInfo.Object.ToString());
			return;
		}

		bool bAppliedData = false;
		GetDataSource().ExtractReplicationDataForObject(Args.ObjectInfo, [this, Object, &bAppliedData](const FConcertSessionSerializedPayload& Payload)
		{
			bAppliedData = true;

			// TODO DP UE-193659: This is very hacky and leaves performance on the table... this is in case ApplyReplicationEvent updates the transform
			if (USceneComponent* SceneComponent = Cast<USceneComponent>(Object))
			{
				ReplicationFormat->ApplyReplicationEvent(*Object, Payload);
				SceneComponent->UpdateComponentToWorld();
			}
			else
			{
				ReplicationFormat->ApplyReplicationEvent(*Object, Payload);
			}
		});
		// This should not happen. If it does, we're wasting  network bandwidth.
		UE_CLOG(!bAppliedData, LogConcert, Warning, TEXT("Replication: Server sent data that could not be applied (likely it was empty) for object %s from stream %s"), *Args.ObjectInfo.Object.ToString(), *Args.ObjectInfo.StreamId.ToString());
	}
}
