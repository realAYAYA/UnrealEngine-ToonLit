// Copyright Epic Games, Inc. All Rights Reserved.

#include "Assets/MultiUserReplicationClientPreset.h"

#include "Assets/MultiUserReplicationStream.h"
#include "Replication/Data/ReplicationStream.h"

UMultiUserReplicationClientPreset::UMultiUserReplicationClientPreset()
{
	Stream = CreateDefaultSubobject<UMultiUserReplicationStream>(TEXT("ReplicationList"));
	Stream->StreamId = MultiUserStreamID;
	Stream->SetFlags(RF_Transactional);
}

void UMultiUserReplicationClientPreset::ClearClient()
{
	Stream->ReplicationMap.ReplicatedObjects.Empty();
}

FConcertReplicationStream UMultiUserReplicationClientPreset::GenerateDescription() const
{
	return Stream->GenerateDescription();
}
