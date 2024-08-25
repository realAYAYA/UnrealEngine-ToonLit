// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IReplicationTreeColumn.h"
#include "ReplicationColumnInfo.h"
#include "Replication/Editor/Model/ReplicatedPropertyData.h"

namespace UE::ConcertSharedSlate
{
	/** Holds information relevant to a row. */
	struct FPropertyTreeRowContext
	{
		/** The data that is stored in the row */
		FReplicatedPropertyData RowData;
	};
	
	using IPropertyTreeColumn = IReplicationTreeColumn<FPropertyTreeRowContext>;
	using FPropertyColumnEntry = TReplicationColumnEntry<FPropertyTreeRowContext>;
}
