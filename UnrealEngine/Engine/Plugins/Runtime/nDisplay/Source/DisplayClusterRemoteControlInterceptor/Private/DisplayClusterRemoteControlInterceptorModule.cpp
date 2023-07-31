// Copyright Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterRemoteControlInterceptorModule.h"
#include "DisplayClusterRemoteControlInterceptorLog.h"
#include "DisplayClusterRemoteControlInterceptor.h"

#include "Modules/ModuleManager.h"
#include "Misc/CoreDelegates.h"
#include "Features/IModularFeatures.h"
#include "IRemoteControlInterceptionFeature.h"
#include "IDisplayCluster.h"

void FDisplayClusterRemoteControlInterceptorModule::StartupModule()
{
	// Instantiate the interceptor feature on module start
	Interceptor = MakeUnique<FDisplayClusterRemoteControlInterceptor>();

	// Register Interceptor only after Engine Loop Complete 
	FCoreDelegates::OnFEngineLoopInitComplete.AddLambda([this]()
	{
		// Add Interceptor only in Cluster mode
		if (IDisplayCluster::Get().GetOperationMode() == EDisplayClusterOperationMode::Cluster)
		{
			// Register the interceptor feature
			IModularFeatures::Get().RegisterModularFeature(IRemoteControlInterceptionFeatureInterceptor::GetName(), Interceptor.Get());	
		}
	});
}

void FDisplayClusterRemoteControlInterceptorModule::ShutdownModule()
{
	// Clean delegates
	FCoreDelegates::OnFEngineLoopInitComplete.RemoveAll(this);

	// Add Interceptor only in Cluster mode
	if (IDisplayCluster::Get().GetOperationMode() == EDisplayClusterOperationMode::Cluster)
	{
		// Unregister the interceptor feature on module shutdown
		IModularFeatures::Get().UnregisterModularFeature(IRemoteControlInterceptionFeatureInterceptor::GetName(), Interceptor.Get());
	}
}

IMPLEMENT_MODULE(FDisplayClusterRemoteControlInterceptorModule, DisplayClusterRemoteControlInterceptor);
