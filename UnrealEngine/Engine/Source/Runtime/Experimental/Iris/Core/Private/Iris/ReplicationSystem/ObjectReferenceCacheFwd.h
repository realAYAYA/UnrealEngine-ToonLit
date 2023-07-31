// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Iris/IrisConstants.h"

class UReplicationSystem;
class UObjectReplicationBridge;

namespace UE::Net
{
	class FNetTokenStoreState;
	class FStringTokenStore;

	struct FNetObjectResolveContext
	{
		FNetTokenStoreState* RemoteNetTokenStoreState = nullptr;
		uint32 ConnectionId = InvalidConnectionId;
	};

	namespace Private
	{
		class FNetExportContext;
	}
}
