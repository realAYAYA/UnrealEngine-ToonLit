// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IReplicationTreeColumn.h"
#include "ReplicationColumnInfo.h"
#include "Replication/Editor/Model/ReplicatedObjectData.h"

namespace UE::ConcertSharedSlate
{
	/** Holds information relevant to a row. */
	struct FObjectTreeRowContext
	{
		/** The data that is stored in the row */
		FReplicatedObjectData RowData;
	};
	
	using IObjectTreeColumn = IReplicationTreeColumn<FObjectTreeRowContext>;
	using FObjectColumnEntry = TReplicationColumnEntry<FObjectTreeRowContext>;
}
