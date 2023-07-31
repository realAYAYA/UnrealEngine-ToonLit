// Copyright Epic Games, Inc. All Rights Reserved.

#include "Policy/MPCDI/DisplayClusterProjectionMPCDIPolicyFactory.h"
#include "Policy/MPCDI/DisplayClusterProjectionMPCDIPolicy.h"
#include "Policy/Mesh/DisplayClusterProjectionMeshPolicy.h"

#include "DisplayClusterProjectionLog.h"
#include "DisplayClusterProjectionStrings.h"

#include "DisplayClusterConfigurationTypes.h"


//////////////////////////////////////////////////////////////////////////////////////////////
// IDisplayClusterProjectionPolicyFactory
//////////////////////////////////////////////////////////////////////////////////////////////
TSharedPtr<IDisplayClusterProjectionPolicy, ESPMode::ThreadSafe> FDisplayClusterProjectionMPCDIPolicyFactory::Create(const FString& ProjectionPolicyId, const FDisplayClusterConfigurationProjection* InConfigurationProjectionPolicy)
{
	check(InConfigurationProjectionPolicy != nullptr);

	UE_LOG(LogDisplayClusterProjectionMPCDI, Verbose, TEXT("Instantiating projection policy <%s>...id='%s'"), *InConfigurationProjectionPolicy->Type, *ProjectionPolicyId);

	if (InConfigurationProjectionPolicy->Type.Equals(DisplayClusterProjectionStrings::projection::MPCDI, ESearchCase::IgnoreCase))
	{
		return MakeShared<FDisplayClusterProjectionMPCDIPolicy, ESPMode::ThreadSafe>(ProjectionPolicyId, InConfigurationProjectionPolicy);
	}

	if (InConfigurationProjectionPolicy->Type.Equals(DisplayClusterProjectionStrings::projection::Mesh, ESearchCase::IgnoreCase))
	{
		return MakeShared<FDisplayClusterProjectionMeshPolicy, ESPMode::ThreadSafe>(ProjectionPolicyId, InConfigurationProjectionPolicy);
	}

	return MakeShared<FDisplayClusterProjectionMPCDIPolicy, ESPMode::ThreadSafe>(ProjectionPolicyId, InConfigurationProjectionPolicy);
};
