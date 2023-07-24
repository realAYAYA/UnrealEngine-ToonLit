// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MeshUVChannelInfo.h"
#include "VertexStreamComponent.h"

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "RenderResource.h"
#include "VertexFactory.h"
#endif

/*=============================================================================
	Components.h: Forward declarations of object components of actors
=============================================================================*/

class FRHIShaderResourceView;


// Constants.
enum { MAX_STATIC_TEXCOORDS = 8 };

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
