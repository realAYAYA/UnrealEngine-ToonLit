// Copyright Epic Games, Inc. All Rights Reserved.

#include "Render/Synchronization/DisplayClusterRenderSyncPolicyNone.h"

#include "Misc/DisplayClusterLog.h"
#include "DisplayClusterConfigurationStrings.h"


bool FDisplayClusterRenderSyncPolicyNone::SynchronizeClusterRendering(int32& InOutSyncInterval)
{
	// Override sync interval with 0 to show a frame ASAP. We don't care about tearing in this policy.
	InOutSyncInterval = 0;
	// Tell a caller that it still needs to present a frame
	return true;
}

FName FDisplayClusterRenderSyncPolicyNone::GetName() const
{
	static const FName NonePolicy = FName(DisplayClusterConfigurationStrings::config::cluster::render_sync::None);
	return NonePolicy;
}
