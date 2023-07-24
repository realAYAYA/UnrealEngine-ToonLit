// Copyright Epic Games, Inc. All Rights Reserved.

#include "Network/Barrier/DisplayClusterBarrierFactory.h"
#include "Network/Barrier/DisplayClusterBarrierV1.h"
#include "Network/Barrier/DisplayClusterBarrierV2.h"

#include "Misc/DisplayClusterLog.h"


TUniquePtr<IDisplayClusterBarrier> FDisplayClusterBarrierFactory::CreateBarrier(const TArray<FString>& ThreadMarkers, const uint32 Timeout, const FString& Name)
{
	// The old barriers v1 are not compatible with failover feature. I left them in the codebase
	// in case it might be useful in the future.
#if 0
	// v1 barrier (old)
	UE_LOG(LogDisplayClusterNetwork, Log, TEXT("Instantiating barrier '%s' of type 'v1': Threads=%d, Timeout=%u ms"), *Name, ThreadsAmount, Timeout);
	return MakeUnique<FDisplayClusterBarrierV1>(ThreadMarkers.Num(), Timeout, Name);
#else
	// v2 barrier (new)
	UE_LOG(LogDisplayClusterNetwork, Log, TEXT("Instantiating barrier '%s' of type 'v2': Threads=%d, Timeout=%u ms"), *Name, ThreadMarkers.Num(), Timeout);
	return MakeUnique<FDisplayClusterBarrierV2>(ThreadMarkers, Timeout, Name);
#endif
}
