// Copyright Epic Games, Inc. All Rights Reserved.
#include "Iris/ReplicationSystem/ReplicationProtocol.h"
#include "HAL/PlatformAtomics.h"

namespace UE::Net
{

void FReplicationProtocol::AddRef() const
{
	++RefCount;
}

void FReplicationProtocol::Release() const
{
	--RefCount;
}

}
