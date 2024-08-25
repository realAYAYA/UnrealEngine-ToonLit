// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

class UVCamComponent;
class UVCamOutputProviderBase;

namespace UE::VCamCore
{
	/** @return Whether the given VCamComponent is allowed to be initialized (e.g. cannot be if is in a Blueprint world) */
	bool CanInitVCamInstance(UVCamComponent* Component);
	/** @return Whether the given VCamComponent is allowed to be initialized (e.g. cannot be if is in a Blueprint world) */
	bool CanInitVCamOutputProvider(UVCamOutputProviderBase* OutputProvider);
}
