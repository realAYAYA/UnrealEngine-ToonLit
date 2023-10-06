// Copyright Epic Games, Inc. All Rights Reserved.

#include "Synchronization/DisplayClusterMediaOutputSynchronizationPolicyEthernetBarrier.h"


FDisplayClusterMediaOutputSynchronizationPolicyEthernetBarrierHandler::FDisplayClusterMediaOutputSynchronizationPolicyEthernetBarrierHandler(UDisplayClusterMediaOutputSynchronizationPolicyEthernetBarrier* InPolicyObject)
	: Super(InPolicyObject)
{

}

TSubclassOf<UDisplayClusterMediaOutputSynchronizationPolicy> FDisplayClusterMediaOutputSynchronizationPolicyEthernetBarrierHandler::GetPolicyClass() const
{
	return UDisplayClusterMediaOutputSynchronizationPolicyEthernetBarrier::StaticClass();
}

void FDisplayClusterMediaOutputSynchronizationPolicyEthernetBarrierHandler::Synchronize()
{
	// Just sync on the barrier
	SyncThreadOnBarrier();
}

TSharedPtr<IDisplayClusterMediaOutputSynchronizationPolicyHandler> UDisplayClusterMediaOutputSynchronizationPolicyEthernetBarrier::GetHandler()
{
	if (!Handler)
	{
		Handler = MakeShared<FDisplayClusterMediaOutputSynchronizationPolicyEthernetBarrierHandler>(this);
	}

	return Handler;
}
