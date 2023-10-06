// Copyright Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterWarpBlendLoader_MeshComponent.h"

#include "DisplayClusterShadersLog.h"

#include "Misc/DisplayClusterHelpers.h"
#include "Misc/FileHelper.h"

#include "Stats/Stats.h"
#include "Engine/Engine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"

#include "WarpBlend/Loader/DisplayClusterWarpBlendLoader_Texture.h"
#include "WarpBlend/DisplayClusterWarpBlend.h"
#include "WarpBlend/DisplayClusterWarpBlend_GeometryContext.h"
#include "WarpBlend/DisplayClusterWarpBlend_GeometryProxy.h"

#include "Render/IDisplayClusterRenderManager.h"

bool FDisplayClusterWarpBlendLoader_MeshComponent::Load(const FDisplayClusterWarpBlendConstruct::FAssignWarpStaticMesh& InParameters, TSharedPtr<IDisplayClusterWarpBlend, ESPMode::ThreadSafe>& OutWarpBlend)
{
	if (InParameters.StaticMeshComponent != nullptr)
	{
		//ok, Create and initialize warpblend interface:
		TSharedPtr<FDisplayClusterWarpBlend, ESPMode::ThreadSafe> WarpBlend = MakeShared<FDisplayClusterWarpBlend, ESPMode::ThreadSafe>();

		WarpBlend->GeometryContext.ProfileType = EDisplayClusterWarpProfileType::warp_A3D;

		FDisplayClusterWarpBlend_GeometryProxy& Proxy = WarpBlend->GeometryContext.GeometryProxy;

		FDisplayClusterMeshUVs MeshUVs(InParameters.BaseUVIndex, InParameters.ChromakeyUVIndex);

		Proxy.MeshComponent = IDisplayCluster::Get().GetRenderMgr()->CreateMeshComponent();
		Proxy.MeshComponent->AssignStaticMeshComponentRefs(InParameters.StaticMeshComponent, MeshUVs, InParameters.OriginComponent, InParameters.StaticMeshComponentLODIndex);

		Proxy.StaticMeshComponentLODIndex = InParameters.StaticMeshComponentLODIndex;
		Proxy.WarpMeshUVs = MeshUVs;

		Proxy.GeometryType = EDisplayClusterWarpGeometryType::WarpMesh;

		OutWarpBlend = WarpBlend;
		return true;
	}

	return false;
}



