// Copyright Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterWarpBlendLoader_ProceduralMeshComponent.h"

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

bool FDisplayClusterWarpBlendLoader_ProceduralMeshComponent::Load(const FDisplayClusterWarpBlendConstruct::FAssignWarpProceduralMesh& InParameters, TSharedPtr<IDisplayClusterWarpBlend, ESPMode::ThreadSafe>& OutWarpBlend)
{
	if (InParameters.ProceduralMeshComponent != nullptr)
	{
		//ok, Create and initialize warpblend interface:
		TSharedPtr<FDisplayClusterWarpBlend, ESPMode::ThreadSafe> WarpBlend = MakeShared<FDisplayClusterWarpBlend, ESPMode::ThreadSafe>();

		WarpBlend->GeometryContext.ProfileType = EDisplayClusterWarpProfileType::warp_A3D;

		FDisplayClusterWarpBlend_GeometryProxy& Proxy = WarpBlend->GeometryContext.GeometryProxy;
		
		FDisplayClusterMeshUVs MeshUVs(InParameters.BaseUVIndex, InParameters.ChromakeyUVIndex);

		// Assign procedural mesh
		Proxy.MeshComponent = IDisplayCluster::Get().GetRenderMgr()->CreateMeshComponent();
		Proxy.MeshComponent->AssignProceduralMeshComponentRefs(InParameters.ProceduralMeshComponent, MeshUVs, InParameters.OriginComponent, InParameters.ProceduralMeshComponentSectionIndex);
		
		Proxy.ProceduralMeshComponentSectionIndex = InParameters.ProceduralMeshComponentSectionIndex;
		Proxy.WarpMeshUVs = MeshUVs;

		Proxy.GeometryType = EDisplayClusterWarpGeometryType::WarpProceduralMesh;

		OutWarpBlend = WarpBlend;
		return true;
	}

	return false;
}



