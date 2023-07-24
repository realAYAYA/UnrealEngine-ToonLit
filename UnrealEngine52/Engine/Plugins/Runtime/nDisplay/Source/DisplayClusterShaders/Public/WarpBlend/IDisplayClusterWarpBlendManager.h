// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "DisplayClusterWarpEnums.h"

class IDisplayClusterWarpBlend;


struct FDisplayClusterWarpBlendConstruct
{
	// Initialize WarpBlend from MPCDI file
	struct FLoadMPCDIFile
	{
		// The file with MPCDI calibration data (extension of this file '.mpcdi')
		FString MPCDIFileName;

		// Name of buffer (mpcdi internal topology)
		FString BufferId;

		// Name of region (mpcdi internal topology)
		FString RegionId;
	};

	// Initialize WarpBlend from PFM file
	struct FLoadPFMFile
	{
		// geometry profile type
		EDisplayClusterWarpProfileType ProfileType = EDisplayClusterWarpProfileType::warp_3D;

		// The file with region calibration data (extension of this file '.pfm')
		FString PFMFileName;

		// Geometry scale of this file
		float   PFMScale = 1.f;

		// Wrap axis to mpcdi space for unreal 
		bool    bIsUnrealGameSpace = false;

		// AlphaMap file (supported format: PNG)
		FString AlphaMapFileName;
		// Control alpha multiplier for AlphaMap texture
		float   AlphaMapEmbeddedAlpha = 0.f;

		// BetaMap file (supported format: PNG)
		FString BetaMapFileName;
	};

	struct FWarpMeshBase
	{
		// Origin component ptr
		class USceneComponent* OriginComponent = nullptr;

		// Map source mesh geometry UVs to WarpMesh UVs
		int32 BaseUVIndex = INDEX_NONE;
		int32 ChromakeyUVIndex = INDEX_NONE;
	};

	struct FAssignWarpStaticMesh : FWarpMeshBase
	{
		// StaticMesh component ptr (warp geometry data source)
		class UStaticMeshComponent* StaticMeshComponent = nullptr;

		// (optional) get geometry data from specified LOD index
		int32 StaticMeshComponentLODIndex = 0;
	};

	struct FAssignWarpProceduralMesh : FWarpMeshBase
	{
		// ProceduralMesh component ptr (warp geometry data source)
		class UProceduralMeshComponent* ProceduralMeshComponent = nullptr;

		// (optional) get geometry data from specified SectionIndex
		int32 ProceduralMeshComponentSectionIndex = 0;
	};
};

class IDisplayClusterWarpBlendManager
{
public:
	virtual ~IDisplayClusterWarpBlendManager() = default;

public:
	virtual bool Create(const FDisplayClusterWarpBlendConstruct::FLoadMPCDIFile&            InConstructParameters, TSharedPtr<IDisplayClusterWarpBlend, ESPMode::ThreadSafe>& OutWarpBlend) const = 0;
	virtual bool Create(const FDisplayClusterWarpBlendConstruct::FLoadPFMFile&              InConstructParameters, TSharedPtr<IDisplayClusterWarpBlend, ESPMode::ThreadSafe>& OutWarpBlend) const = 0;
	virtual bool Create(const FDisplayClusterWarpBlendConstruct::FAssignWarpStaticMesh&     InConstructParameters, TSharedPtr<IDisplayClusterWarpBlend, ESPMode::ThreadSafe>& OutWarpBlend) const = 0;
	virtual bool Create(const FDisplayClusterWarpBlendConstruct::FAssignWarpProceduralMesh& InConstructParameters, TSharedPtr<IDisplayClusterWarpBlend, ESPMode::ThreadSafe>& OutWarpBlend) const = 0;
};
