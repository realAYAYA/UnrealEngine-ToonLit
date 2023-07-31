// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Policy/MPCDI/DisplayClusterProjectionMPCDIPolicy.h"

class UStaticMeshComponent;
class UProceduralMeshComponent;


/*
 * Mesh projection policy
 * Supported geometry sources - StaticMeshComponent, ProceduralMeshComponent
 */
class FDisplayClusterProjectionMeshPolicy
	: public FDisplayClusterProjectionMPCDIPolicy
{
public:
	FDisplayClusterProjectionMeshPolicy(const FString& ProjectionPolicyId, const FDisplayClusterConfigurationProjection* InConfigurationProjectionPolicy);

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IDisplayClusterProjectionPolicy
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual const FString& GetType() const override;
	virtual bool HandleStartScene(IDisplayClusterViewport* InViewport) override;

#if WITH_EDITOR
	virtual bool HasPreviewMesh() override
	{
		return true;
	}

	virtual UMeshComponent* GetOrCreatePreviewMeshComponent(IDisplayClusterViewport* InViewport, bool& bOutIsRootActorComponent) override;
#endif

public:	
	virtual EWarpType GetWarpType() const override
	{
		return EWarpType::mesh;
	}

	/** Parse the config data for a mesh id and try to retrieve it from the root actor. */
	bool CreateWarpMeshInterface(IDisplayClusterViewport* InViewport);

private:
	struct FWarpMeshConfiguration
	{
		// StaticMesh component with source geometry
		UStaticMeshComponent*     StaticMeshComponent = nullptr;
		// StaticMesh geometry LOD
		int32 StaticMeshComponentLODIndex = 0;

		// ProceduralMesh component with source geometry
		UProceduralMeshComponent* ProceduralMeshComponent = nullptr;
		// ProceduralMesh section index
		int32 ProceduralMeshComponentSectionIndex = 0;

		// Customize source geometry UV channels
		int32 BaseUVIndex = INDEX_NONE;
		int32 ChromakeyUVIndex = INDEX_NONE;
	};

	bool GetWarpMeshConfiguration(IDisplayClusterViewport* InViewport, FWarpMeshConfiguration& OutWarpCfg);
};
