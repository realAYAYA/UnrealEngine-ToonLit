// Copyright Epic Games, Inc. All Rights Reserved.

#include "Policy/EasyBlend/DisplayClusterProjectionEasyBlendPolicyFactory.h"
#include "Policy/EasyBlend/DisplayClusterProjectionEasyBlendPolicy.h"

#include "DisplayClusterProjectionLog.h"
#include "DisplayClusterProjectionStrings.h"

#include "DisplayClusterConfigurationTypes.h"
#include "RHI.h"

//////////////////////////////////////////////////////////////////////////////////////////////
// IDisplayClusterProjectionPolicyFactory
//////////////////////////////////////////////////////////////////////////////////////////////
TSharedPtr<IDisplayClusterProjectionPolicy, ESPMode::ThreadSafe> FDisplayClusterProjectionEasyBlendPolicyFactory::Create(const FString& ProjectionPolicyId, const FDisplayClusterConfigurationProjection* InConfigurationProjectionPolicy)
{
	check(InConfigurationProjectionPolicy != nullptr);

	if (FDisplayClusterProjectionEasyBlendPolicy::IsEasyBlendSupported())
	{
		return MakeShared<FDisplayClusterProjectionEasyBlendPolicy, ESPMode::ThreadSafe>(ProjectionPolicyId, InConfigurationProjectionPolicy);
	}

	UE_LOG(LogDisplayClusterProjectionEasyBlend, Warning, TEXT("There is no implementation of '%s' projection policy for RHI %s"), *InConfigurationProjectionPolicy->Type, GDynamicRHI->GetName());
	
	return nullptr;
}
