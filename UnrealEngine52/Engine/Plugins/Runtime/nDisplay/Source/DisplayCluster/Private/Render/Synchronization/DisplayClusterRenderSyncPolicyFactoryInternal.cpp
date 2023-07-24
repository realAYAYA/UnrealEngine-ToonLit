// Copyright Epic Games, Inc. All Rights Reserved.

#include "Render/Synchronization/DisplayClusterRenderSyncPolicyFactoryInternal.h"

#include "Render/Synchronization/DisplayClusterRenderSyncPolicyNone.h"
#include "Render/Synchronization/DisplayClusterRenderSyncPolicyEthernet.h"
#include "Render/Synchronization/DisplayClusterRenderSyncPolicyEthernetBarrier.h"
#include "Render/Synchronization/DisplayClusterRenderSyncPolicyNvidia.h"

#include "Misc/DisplayClusterLog.h"
#include "Misc/DisplayClusterStrings.h"
#include "DisplayClusterConfigurationStrings.h"
#include "RHI.h"
#include "RHIDefinitions.h"

TSharedPtr<IDisplayClusterRenderSyncPolicy> FDisplayClusterRenderSyncPolicyFactoryInternal::Create(const FString& InPolicyType, const TMap<FString, FString>& Parameters)
{
	const ERHIInterfaceType RHIType = RHIGetInterfaceType();

	// None
	if (InPolicyType.Equals(DisplayClusterConfigurationStrings::config::cluster::render_sync::None, ESearchCase::IgnoreCase))
	{
		return MakeShared<FDisplayClusterRenderSyncPolicyNone>(Parameters);
	}
	// EthernetBarrier
	else if (InPolicyType.Equals(DisplayClusterConfigurationStrings::config::cluster::render_sync::EthernetBarrier, ESearchCase::IgnoreCase))
	{
		return MakeShared<FDisplayClusterRenderSyncPolicyEthernetBarrier>(Parameters);
	}
	// Ethernet
	else if (InPolicyType.Equals(DisplayClusterConfigurationStrings::config::cluster::render_sync::Ethernet, ESearchCase::IgnoreCase))
	{
#if PLATFORM_WINDOWS
		if (RHIType == ERHIInterfaceType::D3D11 || RHIType == ERHIInterfaceType::D3D12)
		{
			return MakeShared<FDisplayClusterRenderSyncPolicyEthernet>(Parameters);
		}
		else if (RHIType == ERHIInterfaceType::Vulkan)
		{
			return MakeShared<FDisplayClusterRenderSyncPolicyEthernetBarrier>(Parameters);
		}
#elif PLATFORM_LINUX
		if (RHIType == ERHIInterfaceType::Vulkan)
		{
			return MakeShared<FDisplayClusterRenderSyncPolicyEthernet>(Parameters);
		}
#endif
	}
	// NVIDIA
	else if (InPolicyType.Equals(DisplayClusterConfigurationStrings::config::cluster::render_sync::Nvidia, ESearchCase::IgnoreCase))
	{
#if PLATFORM_WINDOWS
		if (RHIType == ERHIInterfaceType::D3D11 || RHIType == ERHIInterfaceType::D3D12)
		{
			return MakeShared<FDisplayClusterRenderSyncPolicyNvidia>(Parameters);
		}
		else if (RHIType == ERHIInterfaceType::Vulkan)
		{
			UE_LOG(LogDisplayClusterRenderSync, Warning, TEXT("Sync policy '%s' has not been implemented for 'Vulkan' RHI. Default '%s' will be used."),
				DisplayClusterConfigurationStrings::config::cluster::render_sync::Nvidia,
				DisplayClusterConfigurationStrings::config::cluster::render_sync::EthernetBarrier);

			return MakeShared<FDisplayClusterRenderSyncPolicyEthernetBarrier>(Parameters);
		}
#elif PLATFORM_LINUX
		if (RHIType == ERHIInterfaceType::Vulkan)
		{
			UE_LOG(LogDisplayClusterRenderSync, Warning, TEXT("Sync policy '%s' has not been implemented for 'Vulkan' RHI. Default '%s' will be used."),
				DisplayClusterConfigurationStrings::config::cluster::render_sync::Nvidia,
				DisplayClusterConfigurationStrings::config::cluster::render_sync::EthernetBarrier);

			return MakeShared<FDisplayClusterRenderSyncPolicyEthernetBarrier>(Parameters);
		}
#endif
	}

	return nullptr;
}
