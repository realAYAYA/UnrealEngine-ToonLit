// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "RenderResource.h"
#include "VertexFactory.h"
#include "Components.generated.h"

/*=============================================================================
	Components.h: Forward declarations of object components of actors
=============================================================================*/

// Constants.
enum { MAX_TEXCOORDS = 4, MAX_STATIC_TEXCOORDS = 8 };

/** The information used to build a static-mesh vertex. */
struct FStaticMeshBuildVertex
{
	FVector3f Position;

	FVector3f TangentX;
	FVector3f TangentY;
	FVector3f TangentZ;

	FVector2f UVs[MAX_STATIC_TEXCOORDS];
	FColor Color;
};

struct FStaticMeshDataType
{
	/** The stream to read the vertex position from. */
	FVertexStreamComponent PositionComponent;

	/** The streams to read the tangent basis from. */
	FVertexStreamComponent TangentBasisComponents[2];

	/** The streams to read the texture coordinates from. */
	TArray<FVertexStreamComponent, TFixedAllocator<MAX_STATIC_TEXCOORDS / 2> > TextureCoordinates;

	/** The stream to read the shadow map texture coordinates from. */
	FVertexStreamComponent LightMapCoordinateComponent;

	/** The stream to read the vertex color from. */
	FVertexStreamComponent ColorComponent;

	FRHIShaderResourceView* PositionComponentSRV = nullptr;

	FRHIShaderResourceView* TangentsSRV = nullptr;

	/** A SRV to manually bind and load TextureCoordinates in the Vertexshader. */
	FRHIShaderResourceView* TextureCoordinatesSRV = nullptr;

	/** A SRV to manually bind and load Colors in the Vertexshader. */
	FRHIShaderResourceView* ColorComponentsSRV = nullptr;

	int LightMapCoordinateIndex = -1;
	int NumTexCoords = -1;
	uint32 ColorIndexMask = ~0u;
	uint32 LODLightmapDataIndex = 0;
};

/** The world size for each texcoord mapping. Used by the texture streaming. */
USTRUCT(BlueprintType)
struct FMeshUVChannelInfo
{
	GENERATED_USTRUCT_BODY()

	/** Default constructor (no initialization). */
	FORCEINLINE FMeshUVChannelInfo() { FMemory::Memzero(*this); }

	/** Constructor which initializes all components to zero. */
	FMeshUVChannelInfo(ENoInit) { }

	FMeshUVChannelInfo(float DefaultDensity) : bInitialized(true), bOverrideDensities(false)
	{
		for (float& Density : LocalUVDensities)
		{
			Density = DefaultDensity;
		}
	}

	/** Returns whether the structure contains any valid LocalUVDensities. */
	bool IsInitialized() const
	{
		if (bInitialized)
		{
			for (float Density : LocalUVDensities)
			{
				if (Density != 0)
				{
					return true;
				}
			}
		}
		return false;
	}

	UPROPERTY()
	bool bInitialized;

	/** Whether this values was set manually or is auto generated. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = Basic)
	bool bOverrideDensities;

	/**
	 * The UV density in the mesh, before any transform scaling, in world unit per UV.
	 * This value represents the length taken to cover a full UV unit.
	 */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = Basic, meta = (EditCondition = "bOverrideDensities"))
	float LocalUVDensities[MAX_TEXCOORDS];

	friend FArchive& operator<<(FArchive& Ar, FMeshUVChannelInfo& Info);
};
