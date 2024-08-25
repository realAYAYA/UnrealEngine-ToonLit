// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IReplicationTreeColumn.h"

namespace UE::ConcertSharedSlate
{
	template<typename TTreeItemType>
	struct TReplicationColumnDelegates
	{
		DECLARE_DELEGATE_RetVal(TSharedRef<IReplicationTreeColumn<TTreeItemType>>, FCreateColumn);
	};
}
