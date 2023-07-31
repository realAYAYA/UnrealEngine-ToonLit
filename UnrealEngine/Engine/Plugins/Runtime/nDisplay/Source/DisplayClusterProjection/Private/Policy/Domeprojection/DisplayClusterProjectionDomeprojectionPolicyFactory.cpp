// Copyright Epic Games, Inc. All Rights Reserved.

#include "Policy/Domeprojection/DisplayClusterProjectionDomeprojectionPolicyFactory.h"

#include "DisplayClusterProjectionLog.h"
#include "Misc/DisplayClusterStrings.h"

#include "DisplayClusterConfigurationTypes.h"

#if PLATFORM_WINDOWS
#include "Policy/Domeprojection/Windows/DX11/DisplayClusterProjectionDomeprojectionPolicyDX11.h"
#endif


//////////////////////////////////////////////////////////////////////////////////////////////
// IDisplayClusterProjectionPolicyFactory
//////////////////////////////////////////////////////////////////////////////////////////////
TSharedPtr<IDisplayClusterProjectionPolicy, ESPMode::ThreadSafe> FDisplayClusterProjectionDomeprojectionPolicyFactory::Create(const FString& ProjectionPolicyId, const FDisplayClusterConfigurationProjection* InConfigurationProjectionPolicy)
{
	const ERHIInterfaceType RHIType = RHIGetInterfaceType();

#if PLATFORM_WINDOWS
	if (RHIType == ERHIInterfaceType::D3D11)
	{
		check(InConfigurationProjectionPolicy != nullptr);

		UE_LOG(LogDisplayClusterProjectionDomeprojection, Verbose, TEXT("Instantiating projection policy <%s> id='%s'"), *InConfigurationProjectionPolicy->Type, *ProjectionPolicyId);
		return MakeShared<FDisplayClusterProjectionDomeprojectionPolicyDX11, ESPMode::ThreadSafe>(ProjectionPolicyId, InConfigurationProjectionPolicy);
	}
#endif

	UE_LOG(LogDisplayClusterProjectionDomeprojection, Warning, TEXT("There is no implementation of '%s' projection policy for RHI %s"), *InConfigurationProjectionPolicy->Type, GDynamicRHI->GetName());

	return nullptr;
}
