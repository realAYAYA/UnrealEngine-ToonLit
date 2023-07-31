// Copyright Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterWarpBlendManager.h"

#include "WarpBlend/Loader/DisplayClusterWarpBlendLoader_MPCDI.h"
#include "WarpBlend/Loader/DisplayClusterWarpBlendLoader_MeshComponent.h"
#include "WarpBlend/Loader/DisplayClusterWarpBlendLoader_ProceduralMeshComponent.h"

bool FDisplayClusterWarpBlendManager::Create(const FDisplayClusterWarpBlendConstruct::FLoadMPCDIFile& InConstructParameters, TSharedPtr<IDisplayClusterWarpBlend, ESPMode::ThreadSafe>& OutWarpBlend) const
{
	return FDisplayClusterWarpBlendLoader_MPCDI::Load(InConstructParameters, OutWarpBlend);
}

bool FDisplayClusterWarpBlendManager::Create(const FDisplayClusterWarpBlendConstruct::FLoadPFMFile& InConstructParameters, TSharedPtr<IDisplayClusterWarpBlend, ESPMode::ThreadSafe>& OutWarpBlend) const
{
	return FDisplayClusterWarpBlendLoader_MPCDI::Load(InConstructParameters, OutWarpBlend);
}

bool  FDisplayClusterWarpBlendManager::Create(const FDisplayClusterWarpBlendConstruct::FAssignWarpStaticMesh& InConstructParameters, TSharedPtr<IDisplayClusterWarpBlend, ESPMode::ThreadSafe>& OutWarpBlend) const
{
	return FDisplayClusterWarpBlendLoader_MeshComponent::Load(InConstructParameters, OutWarpBlend);
};

bool  FDisplayClusterWarpBlendManager::Create(const FDisplayClusterWarpBlendConstruct::FAssignWarpProceduralMesh& InConstructParameters, TSharedPtr<IDisplayClusterWarpBlend, ESPMode::ThreadSafe>& OutWarpBlend) const
{
	return FDisplayClusterWarpBlendLoader_ProceduralMeshComponent::Load(InConstructParameters, OutWarpBlend);
};

