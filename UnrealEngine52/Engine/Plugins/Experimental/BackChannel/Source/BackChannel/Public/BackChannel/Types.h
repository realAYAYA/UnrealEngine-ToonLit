// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Delegates/Delegate.h"

class IBackChannelConnection;
class IBackChannelPacket;

struct FBackChannelPacketType
{
	FBackChannelPacketType(char A, char B, char C, char D)
		: ID{ A,B,C,D }
	{
		FMemory::Memzero(ID);
	}
	char ID[4];
};


DECLARE_MULTICAST_DELEGATE_OneParam(FBackChannelRouteDelegate, IBackChannelPacket&)

template <class ObjectType>
using TBackChannelSharedPtr = TSharedPtr<ObjectType, ESPMode::ThreadSafe>;

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
