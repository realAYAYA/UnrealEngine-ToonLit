// Copyright Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterWarpModule.h"

#include "DisplayClusterWarpLog.h"
#include "DisplayClusterWarpStrings.h"

#include "IDisplayCluster.h"
#include "Render/IDisplayClusterRenderManager.h"
#include "Policy/InFrustumFit/DisplayClusterWarpInFrustumFitPolicyFactory.h"

//////////////////////////////////////////////////////////////////////////////////////////////
// FDisplayClusterWarpModule
//////////////////////////////////////////////////////////////////////////////////////////////
FDisplayClusterWarpModule::FDisplayClusterWarpModule()
{
	TSharedPtr<IDisplayClusterWarpPolicyFactory> Factory;

	// InFrustumFit warp policy
	Factory = MakeShared<FDisplayClusterWarpInFrustumFitPolicyFactory>();
	WarpPolicyFactories.Emplace(UE::DisplayClusterWarpStrings::warp::InFrustumFit, Factory);

	UE_LOG(LogDisplayClusterWarp, Log, TEXT("Warp module has been instantiated"));
}

FDisplayClusterWarpModule::~FDisplayClusterWarpModule()
{
	UE_LOG(LogDisplayClusterWarp, Log, TEXT("Warp module has been destroyed"));
}

void FDisplayClusterWarpModule::StartupModule()
{
	UE_LOG(LogDisplayClusterWarp, Log, TEXT("Warp module startup"));

	IDisplayClusterRenderManager* RenderMgr = IDisplayCluster::Get().GetRenderMgr();
	if (RenderMgr)
	{
		for (auto it = WarpPolicyFactories.CreateIterator(); it; ++it)
		{
			UE_LOG(LogDisplayClusterWarp, Log, TEXT("Registering <%s> warp policy factory..."), *it->Key);

			if (!RenderMgr->RegisterWarpPolicyFactory(it->Key, it->Value))
			{
				UE_LOG(LogDisplayClusterWarp, Warning, TEXT("Couldn't register <%s> warp policy factory"), *it->Key);
			}
		}
	}

	UE_LOG(LogDisplayClusterWarp, Log, TEXT("Warp module has started"));
}

void FDisplayClusterWarpModule::ShutdownModule()
{
	UE_LOG(LogDisplayClusterWarp, Log, TEXT("Warp module shutdown"));

	IDisplayClusterRenderManager* RenderMgr = IDisplayCluster::Get().GetRenderMgr();
	if (RenderMgr)
	{
		for (auto it = WarpPolicyFactories.CreateConstIterator(); it; ++it)
		{
			UE_LOG(LogDisplayClusterWarp, Log, TEXT("Un-registering <%s> warp factory..."), *it->Key);

			if (!RenderMgr->UnregisterWarpPolicyFactory(it->Key))
			{
				UE_LOG(LogDisplayClusterWarp, Warning, TEXT("An error occurred during un-registering the <%s> warp factory"), *it->Key);
			}
		}
	}

	UE_LOG(LogDisplayClusterWarp, Log, TEXT("Warp module has been shutdown."));
}

IMPLEMENT_MODULE(FDisplayClusterWarpModule, DisplayClusterWarp);
