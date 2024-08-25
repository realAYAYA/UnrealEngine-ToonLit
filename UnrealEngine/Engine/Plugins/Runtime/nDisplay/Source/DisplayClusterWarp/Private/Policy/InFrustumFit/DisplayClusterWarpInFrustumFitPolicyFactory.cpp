// Copyright Epic Games, Inc. All Rights Reserved.

#include "Policy/InFrustumFit/DisplayClusterWarpInFrustumFitPolicyFactory.h"
#include "Policy/InFrustumFit/DisplayClusterWarpInFrustumFitPolicy.h"

#include "DisplayClusterWarpStrings.h"
#include "DisplayClusterWarpLog.h"

//////////////////////////////////////////////////////////////////////////////////////////////
// FDisplayClusterWarpCameraFitPolicyFactory
//////////////////////////////////////////////////////////////////////////////////////////////
TSharedPtr<IDisplayClusterWarpPolicy, ESPMode::ThreadSafe> FDisplayClusterWarpInFrustumFitPolicyFactory::Create(const FString& InWarpPolicyType, const FString& InWarpPolicyName)
{
	TSharedPtr<IDisplayClusterWarpPolicy, ESPMode::ThreadSafe> Result;

	if (InWarpPolicyType == UE::DisplayClusterWarpStrings::warp::InFrustumFit)
	{
		Result = MakeShared<FDisplayClusterWarpInFrustumFitPolicy, ESPMode::ThreadSafe>(InWarpPolicyName);
	}

	if (Result.IsValid())
	{
		UE_LOG(LogDisplayClusterWarpInFrustumFitPolicy, Verbose, TEXT("Instantiating warp policy <%s> id='%s'"), *InWarpPolicyType, *InWarpPolicyName);
	}

	return  Result;
}
