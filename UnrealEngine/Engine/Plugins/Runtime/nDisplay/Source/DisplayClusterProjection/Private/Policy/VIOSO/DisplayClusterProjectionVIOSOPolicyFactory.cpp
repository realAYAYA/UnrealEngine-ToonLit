// Copyright Epic Games, Inc. All Rights Reserved.

#include "Policy/VIOSO/DisplayClusterProjectionVIOSOPolicyFactory.h"

#include "DisplayClusterProjectionStrings.h"
#include "DisplayClusterProjectionLog.h"

#include "DisplayClusterConfigurationTypes.h"

#if PLATFORM_WINDOWS
#include "Policy/VIOSO/Windows/DisplayClusterProjectionVIOSOPolicy.h"
#endif

#include "Policy/VIOSO/DisplayClusterProjectionVIOSOLibrary.h"

//////////////////////////////////////////////////////////////////////////////////////////////
// IDisplayClusterProjectionPolicyFactory
//////////////////////////////////////////////////////////////////////////////////////////////
FDisplayClusterProjectionVIOSOPolicyFactory::FDisplayClusterProjectionVIOSOPolicyFactory()
{
	VIOSOLibrary = MakeShared<FDisplayClusterProjectionVIOSOLibrary, ESPMode::ThreadSafe>();
}

TSharedPtr<IDisplayClusterProjectionPolicy, ESPMode::ThreadSafe> FDisplayClusterProjectionVIOSOPolicyFactory::Create(const FString& ProjectionPolicyId, const FDisplayClusterConfigurationProjection* InConfigurationProjectionPolicy)
{
	check(InConfigurationProjectionPolicy != nullptr);
	if (!VIOSOLibrary.IsValid() || !VIOSOLibrary->IsInitialized())
	{
		UE_LOG(LogDisplayClusterProjectionVIOSO, Error, TEXT("VIOSO API not initialized: cannot instantiate projection policy <%s> id='%s'"), *InConfigurationProjectionPolicy->Type, *ProjectionPolicyId);
		return nullptr;
	}

#if PLATFORM_WINDOWS
	UE_LOG(LogDisplayClusterProjectionVIOSO, Verbose, TEXT("Instantiating projection policy <%s> id='%s'"), *InConfigurationProjectionPolicy->Type, *ProjectionPolicyId);

	return  MakeShared<FDisplayClusterProjectionVIOSOPolicy, ESPMode::ThreadSafe>(ProjectionPolicyId, InConfigurationProjectionPolicy, VIOSOLibrary.ToSharedRef());
#endif

	UE_LOG(LogDisplayClusterProjectionVIOSO, Error, TEXT("VIOSO does not support the current platform: cannot instantiate projection policy <%s> id='%s'"), *InConfigurationProjectionPolicy->Type, *ProjectionPolicyId);
	return nullptr;
}
