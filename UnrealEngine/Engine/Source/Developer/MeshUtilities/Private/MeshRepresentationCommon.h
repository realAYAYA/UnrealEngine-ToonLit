// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MeshUtilities.h"
#include "kDOP.h"

#if USE_EMBREE
	#include <embree3/rtcore.h>
	#include <embree3/rtcore_ray.h>
#else
	typedef void* RTCDevice;
	typedef void* RTCScene;
	typedef void* RTCGeometry;
#endif

class FSourceMeshDataForDerivedDataTask;

class FMeshBuildDataProvider
{
public:

	/** Initialization constructor. */
	FMeshBuildDataProvider(
		const TkDOPTree<const FMeshBuildDataProvider, uint32>& InkDopTree) :
		kDopTree(InkDopTree)
	{}

	// kDOP data provider interface.

	FORCEINLINE const TkDOPTree<const FMeshBuildDataProvider, uint32>& GetkDOPTree(void) const
	{
		return kDopTree;
	}

	FORCEINLINE const FMatrix& GetLocalToWorld(void) const
	{
		return FMatrix::Identity;
	}

	FORCEINLINE const FMatrix& GetWorldToLocal(void) const
	{
		return FMatrix::Identity;
	}

	FORCEINLINE FMatrix GetLocalToWorldTransposeAdjoint(void) const
	{
		return FMatrix::Identity;
	}

	FORCEINLINE float GetDeterminant(void) const
	{
		return 1.0f;
	}

private:

	const TkDOPTree<const FMeshBuildDataProvider, uint32>& kDopTree;
};

struct FEmbreeTriangleDesc
{
	int16 ElementIndex;

	bool IsTwoSided() const
	{
		// MaterialIndex on the build triangles was set to 1 if two-sided, or 0 if one-sided
		return ElementIndex == 1;
	}
};

// Mapping between Embree Geometry Id and engine Mesh/LOD Id
struct FEmbreeGeometry
{
	TArray<uint32> IndexArray;
	TArray<FVector3f> VertexArray;
	TArray<FEmbreeTriangleDesc> TriangleDescs; // The material ID of each triangle.
	RTCGeometry InternalGeometry;
};

class FEmbreeScene
{
public:
	bool bUseEmbree = false;
	int32 NumIndices = 0;
	bool bMostlyTwoSided = false;

	// Embree
	RTCDevice EmbreeDevice = nullptr;
	RTCScene EmbreeScene = nullptr;
	FEmbreeGeometry Geometry;

	// DOP tree fallback
	TkDOPTree<const FMeshBuildDataProvider, uint32> kDopTree;
};

#if USE_EMBREE
struct FEmbreeRay : public RTCRayHit
{
	FEmbreeRay() :
		ElementIndex(-1)
	{
		hit.u = hit.v = 0;
		ray.time = 0;
		ray.mask = 0xFFFFFFFF;
		hit.geomID = RTC_INVALID_GEOMETRY_ID;
		hit.instID[0] = RTC_INVALID_GEOMETRY_ID;
		hit.primID = RTC_INVALID_GEOMETRY_ID;
	}

	FVector3f GetHitNormal() const
	{
		return FVector3f(-hit.Ng_x, -hit.Ng_y, -hit.Ng_z).GetSafeNormal();
	}	

	bool IsHitTwoSided() const
	{
		// MaterialIndex on the build triangles was set to 1 if two-sided, or 0 if one-sided
		return ElementIndex == 1;
	}

	// Additional Outputs.
	int32 ElementIndex; // Material Index
};

struct FEmbreeIntersectionContext : public RTCIntersectContext
{
	FEmbreeIntersectionContext() :
		ElementIndex(-1)
	{}

	bool IsHitTwoSided() const
	{
		// MaterialIndex on the build triangles was set to 1 if two-sided, or 0 if one-sided
		return ElementIndex == 1;
	}

	// Hit against this primitive will be ignored
	int32 SkipPrimId = RTC_INVALID_GEOMETRY_ID;

	// Additional Outputs.
	int32 ElementIndex; // Material Index
};

#endif

namespace MeshRepresentation
{
	/**
	 *	Generates unit length, stratified and uniformly distributed direction samples in a hemisphere. 
	 */
	void GenerateStratifiedUniformHemisphereSamples(int32 NumSamples, FRandomStream& RandomStream, TArray<FVector4>& Samples);

	/**
	 *	[Frisvad 2012, "Building an Orthonormal Basis from a 3D Unit Vector Without Normalization"]
	 */
	FMatrix44f GetTangentBasisFrisvad(FVector3f TangentZ);

	void SetupEmbreeScene(FString MeshName,
		const FSourceMeshDataForDerivedDataTask& SourceMeshData,
		const FStaticMeshLODResources& LODModel,
		const TArray<FSignedDistanceFieldBuildSectionData>& SectionData,
		bool bGenerateAsIfTwoSided,
		bool bIncludeTranslucentTriangles,
		FEmbreeScene& EmbreeScene);

	void DeleteEmbreeScene(FEmbreeScene& EmbreeScene);
};
