// Copyright Epic Games, Inc. All Rights Reserved.

#include "Policy/Mesh/DisplayClusterProjectionMeshPolicy.h"
#include "DisplayClusterProjectionLog.h"
#include "DisplayClusterProjectionStrings.h"
#include "DisplayClusterRootActor.h"

#include "Misc/DisplayClusterHelpers.h"

#include "Render/Viewport/IDisplayClusterViewportManager.h"
#include "Render/Viewport/IDisplayClusterViewport.h"

#include "IDisplayClusterShaders.h"
#include "WarpBlend/IDisplayClusterWarpBlend.h"
#include "WarpBlend/IDisplayClusterWarpBlendManager.h"

#include "Components/StaticMeshComponent.h"
#include "ProceduralMeshComponent.h"


FDisplayClusterProjectionMeshPolicy::FDisplayClusterProjectionMeshPolicy(const FString& ProjectionPolicyId, const FDisplayClusterConfigurationProjection* InConfigurationProjectionPolicy)
	: FDisplayClusterProjectionMPCDIPolicy(ProjectionPolicyId, InConfigurationProjectionPolicy)
{
}

bool FDisplayClusterProjectionMeshPolicy::CreateWarpMeshInterface(IDisplayClusterViewport* InViewport)
{
	check(IsInGameThread());

	if (WarpBlendInterface.IsValid() == false)
	{
		// Read configuration from config
		FWarpMeshConfiguration WarpCfg;
		if (!GetWarpMeshConfiguration(InViewport, WarpCfg))
		{
			return false;
		}

		// The mesh always uses DCRootActor as its origin because all geometry is in UE space.
		InitializeOriginComponent(InViewport, TEXT(""));

		IDisplayClusterShaders& ShadersAPI = IDisplayClusterShaders::Get();
		bool bResult = false;

		if (WarpCfg.StaticMeshComponent != nullptr)
		{
			FDisplayClusterWarpBlendConstruct::FAssignWarpStaticMesh CreateParameters;

			CreateParameters.OriginComponent = GetOriginComp();

			CreateParameters.StaticMeshComponent = WarpCfg.StaticMeshComponent;
			CreateParameters.StaticMeshComponentLODIndex = WarpCfg.StaticMeshComponentLODIndex;

			CreateParameters.BaseUVIndex      = WarpCfg.BaseUVIndex;
			CreateParameters.ChromakeyUVIndex = WarpCfg.ChromakeyUVIndex;

			bResult = ShadersAPI.GetWarpBlendManager().Create(CreateParameters, WarpBlendInterface);
		}
		else
		{
			FDisplayClusterWarpBlendConstruct::FAssignWarpProceduralMesh CreateParameters;

			CreateParameters.OriginComponent = GetOriginComp();

			CreateParameters.ProceduralMeshComponent = WarpCfg.ProceduralMeshComponent;
			CreateParameters.ProceduralMeshComponentSectionIndex = WarpCfg.ProceduralMeshComponentSectionIndex;

			CreateParameters.BaseUVIndex      = WarpCfg.BaseUVIndex;
			CreateParameters.ChromakeyUVIndex = WarpCfg.ChromakeyUVIndex;

			bResult = ShadersAPI.GetWarpBlendManager().Create(CreateParameters, WarpBlendInterface);
		}

		if (!bResult)
		{
			if (!IsEditorOperationMode(InViewport))
			{
				UE_LOG(LogDisplayClusterProjectionMesh, Warning, TEXT("Couldn't create mesh warpblend interface"));
			}

			return false;
		}
	}

	return true;
}

const FString& FDisplayClusterProjectionMeshPolicy::GetType() const
{
	static const FString Type(DisplayClusterProjectionStrings::projection::Mesh);
	return Type;
}

bool FDisplayClusterProjectionMeshPolicy::HandleStartScene(IDisplayClusterViewport* InViewport)
{
	check(IsInGameThread());

	// The game side of the nDisplay has been initialized by the nDisplay Game Manager already
	// so we can extend it by our projection related functionality/components/etc.

	WarpBlendContexts.Empty();

	if (!CreateWarpMeshInterface(InViewport))
	{
		if (!IsEditorOperationMode(InViewport))
		{
			UE_LOG(LogDisplayClusterProjectionMesh, Error, TEXT("Couldn't create warp interface for viewport '%s'"), *InViewport->GetId());
		}

		return false;
	}

	// Finally, initialize internal views data container
	WarpBlendContexts.AddDefaulted(2);

	return true;
}

bool FDisplayClusterProjectionMeshPolicy::GetWarpMeshConfiguration(IDisplayClusterViewport* InViewport, FWarpMeshConfiguration& OutWarpCfg)
{
	check(InViewport);

	// Get our VR root
	ADisplayClusterRootActor* const Root = InViewport->GetOwner().GetRootActor();
	if (!Root)
	{
		UE_LOG(LogDisplayClusterProjectionMesh, Error, TEXT("Couldn't get a VR root object"));
		return false;
	}

	FString ComponentId;
	// Get assigned mesh ID
	if (!DisplayClusterHelpers::map::template ExtractValue(GetParameters(), DisplayClusterProjectionStrings::cfg::mesh::Component, ComponentId))
	{

#if WITH_EDITOR
		if (ComponentId.IsEmpty())
		{
			return false;
		}
#endif

		if (!IsEditorOperationMode(InViewport))
		{
			UE_LOG(LogDisplayClusterProjectionMesh, Error, TEXT("No component ID '%s' specified for projection policy '%s'"), *ComponentId, *GetId());
		}

		return false;
	}

	int32 CfgBaseUVIndex = INDEX_NONE;
	if (DisplayClusterHelpers::map::template ExtractValueFromString(GetParameters(), DisplayClusterProjectionStrings::cfg::mesh::BaseUVIndex, CfgBaseUVIndex))
	{
		if (CfgBaseUVIndex >= 0)
		{
			UE_LOG(LogDisplayClusterProjectionMesh, Verbose, TEXT("Found BaseUVIndex value - '%d'"), CfgBaseUVIndex);
		}
		else if(CfgBaseUVIndex != INDEX_NONE)
		{
			UE_LOG(LogDisplayClusterProjectionMesh, Error, TEXT("Invalid BaseUVIndex value - '%d'"), CfgBaseUVIndex);
			CfgBaseUVIndex = INDEX_NONE;
		}
	}
	OutWarpCfg.BaseUVIndex = CfgBaseUVIndex;

	int32 CfgChromakeyUVIndex = INDEX_NONE;
	if (DisplayClusterHelpers::map::template ExtractValueFromString(GetParameters(), DisplayClusterProjectionStrings::cfg::mesh::ChromakeyUVIndex, CfgChromakeyUVIndex))
	{
		if (CfgChromakeyUVIndex >= 0)
		{
			UE_LOG(LogDisplayClusterProjectionMesh, Verbose, TEXT("Found ChromakeyUVIndex value - '%d'"), CfgChromakeyUVIndex);
		}
		else if(CfgChromakeyUVIndex != INDEX_NONE)
		{
			UE_LOG(LogDisplayClusterProjectionMesh, Error, TEXT("Invalid ChromakeyUVIndex value - '%d'"), CfgChromakeyUVIndex);
			CfgChromakeyUVIndex = INDEX_NONE;
		}
	}
	OutWarpCfg.ChromakeyUVIndex = CfgChromakeyUVIndex;

	// Get the StaticMeshComponent
	OutWarpCfg.StaticMeshComponent = Root->GetComponentByName<UStaticMeshComponent>(ComponentId);
	if (OutWarpCfg.StaticMeshComponent != nullptr)
	{
		int CfgLODIndex;
		if (DisplayClusterHelpers::map::template ExtractValueFromString(GetParameters(), DisplayClusterProjectionStrings::cfg::mesh::LODIndex, CfgLODIndex))
		{
			if (CfgLODIndex >= 0)
			{
				UE_LOG(LogDisplayClusterProjectionMesh, Verbose, TEXT("Found StaticMeshComponent LODIndex value - '%d'"), CfgLODIndex);

				OutWarpCfg.StaticMeshComponentLODIndex = CfgLODIndex;
			}
			else
			{
				UE_LOG(LogDisplayClusterProjectionMesh, Error, TEXT("Invalid StaticMeshComponent LODIndex value - '%d'"), CfgLODIndex);
			}
		}
	}
	else
	{
		// Get the ProceduralMeshComponent
		OutWarpCfg.ProceduralMeshComponent = Root->GetComponentByName<UProceduralMeshComponent>(ComponentId);
		if (OutWarpCfg.ProceduralMeshComponent == nullptr)
		{
			if (!IsEditorOperationMode(InViewport))
			{
				UE_LOG(LogDisplayClusterProjectionMesh, Warning, TEXT("Couldn't find mesh component '%s' in RootActor"), *ComponentId);
			}

			return false;
		}

		int32 CfgSectionIndex;
		if (DisplayClusterHelpers::map::template ExtractValueFromString(GetParameters(), DisplayClusterProjectionStrings::cfg::mesh::SectionIndex, CfgSectionIndex))
		{
			if (CfgSectionIndex >= 0)
			{
				UE_LOG(LogDisplayClusterProjectionMesh, Verbose, TEXT("Found ProceduralMeshComponent SectionIndex value - '%d'"), CfgSectionIndex);

				OutWarpCfg.ProceduralMeshComponentSectionIndex = CfgSectionIndex;
			}
			else
			{
				UE_LOG(LogDisplayClusterProjectionMesh, Error, TEXT("Invalid ProceduralMeshComponent SectionIndex value - '%d'"), CfgSectionIndex);
			}
		}
	}

	return true;
}

#if WITH_EDITOR
UMeshComponent* FDisplayClusterProjectionMeshPolicy::GetOrCreatePreviewMeshComponent(IDisplayClusterViewport* InViewport, bool& bOutIsRootActorComponent)
{
	bOutIsRootActorComponent = true;

	FWarpMeshConfiguration WarpCfg;
	if (GetWarpMeshConfiguration(InViewport, WarpCfg))
	{
		if(WarpCfg.StaticMeshComponent != nullptr)
		{
			return WarpCfg.StaticMeshComponent;
		}

		if (WarpCfg.ProceduralMeshComponent != nullptr)
		{
			return WarpCfg.ProceduralMeshComponent;
		}
	}

	return nullptr;
}
#endif

