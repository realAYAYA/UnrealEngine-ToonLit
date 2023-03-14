// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "RHIDefinitions.h"
#include "PackedNormal.h"
#include "RenderGraphResources.h"
#include "Serialization/BulkData.h"

DECLARE_LOG_CATEGORY_EXTERN(LogHairStrands, Log, All);

struct FPackedHairVertex
{
	typedef uint64 BulkType;

	FFloat16 X, Y, Z;
	uint8 PackedRadiusAndType;
	uint8 UCoord;
};

struct FPackedHairAttribute0Vertex
{
	typedef uint16 BulkType;

	uint8 NormalizedLength;
	uint8 Seed;
};

struct FPackedHairAttribute1Vertex
{
	typedef uint32 BulkType;

	uint32 Packed;
};

struct FHairMaterialVertex
{
	typedef uint32 BulkType;

	// sRGB color space
	uint8 BaseColorR;
	uint8 BaseColorG;
	uint8 BaseColorB;

	uint8 Roughness;
};

struct FHairInterpolationVertex
{
	typedef uint32 BulkType;

	// Guide's vertex index are stored onto 24bits: 1) VertexGuideIndex0 is the lower part, 2) VertexGuideIndex1 is the upper part
	uint16 VertexGuideIndex0;
	uint8  VertexGuideIndex1;
	uint8  VertexLerp;
};

struct FHairInterpolation0Vertex
{
	typedef uint64 BulkType;

	uint16 Index0;
	uint16 Index1;
	uint16 Index2;

	uint8 VertexWeight0;
	uint8 VertexWeight1;
};

struct FHairInterpolation1Vertex
{
	typedef uint64 BulkType;

	uint8 VertexIndex0;
	uint8 VertexIndex1;
	uint8 VertexIndex2;

	uint8 VertexLerp0;
	uint8 VertexLerp1;
	uint8 VertexLerp2;

	uint8 Pad0;
	uint8 Pad1;
};

struct FVector4_16
{
	FFloat16 X;
	FFloat16 Y;
	FFloat16 Z;
	FFloat16 W;
}; 
FArchive& operator<<(FArchive& Ar, FVector4_16& Vertex);

struct FHairStrandsPositionFormat
{
	typedef FPackedHairVertex Type;
	typedef FPackedHairVertex::BulkType BulkType;
	static const uint32 ComponentCount = 1;
	static const uint32 SizeInByte = sizeof(Type);
	static const EVertexElementType VertexElementType = VET_UShort4;
	static const EPixelFormat Format = PF_R16G16B16A16_UINT;
};

struct FHairStrandsPositionOffsetFormat
{
	typedef FVector4f Type;
	static const uint32 ComponentCount = 1;
	static const uint32 SizeInByte = sizeof(Type);
	static const EVertexElementType VertexElementType = VET_Float4;
	static const EPixelFormat Format = PF_A32B32G32R32F;
};

struct FHairStrandsAttribute0Format
{
	typedef FPackedHairAttribute0Vertex Type;
	typedef FPackedHairAttribute0Vertex::BulkType BulkType;
	static const uint32 ComponentCount = 1;
	static const uint32 SizeInByte = sizeof(Type);
	static const EVertexElementType VertexElementType = VET_UByte4;
	static const EPixelFormat Format = PF_R8G8;
};

struct FHairStrandsAttribute1Format
{
	typedef FPackedHairAttribute1Vertex Type;
	typedef FPackedHairAttribute1Vertex::BulkType BulkType;
	static const uint32 ComponentCount = 1;
	static const uint32 SizeInByte = sizeof(Type);
	static const EVertexElementType VertexElementType = VET_UInt;
	static const EPixelFormat Format = PF_R32_UINT;
};

struct FHairStrandsMaterialFormat
{
	typedef FHairMaterialVertex Type;
	typedef FHairMaterialVertex::BulkType BulkType;
	static const uint32 ComponentCount = 1;
	static const uint32 SizeInByte = sizeof(Type);
	static const EVertexElementType VertexElementType = VET_UByte4;
	static const EPixelFormat Format = PF_R8G8B8A8;
};

struct FHairStrandsTangentFormat
{
	typedef FPackedNormal Type;
	typedef uint32 BulkType;
	// TangentX & tangentZ are packed into 2 * PF_R8G8B8A8_SNORM
	static const uint32 ComponentCount = 2;
	static const uint32 SizeInByte = sizeof(Type);
	static const EVertexElementType VertexElementType = VET_Float4;
	static const EPixelFormat Format = PF_R8G8B8A8_SNORM;
};

struct FHairStrandsInterpolationFormat
{
	typedef FHairInterpolationVertex Type;
	typedef FHairInterpolationVertex::BulkType BulkType;

	static const uint32 ComponentCount = 1;
	static const uint32 SizeInByte = sizeof(Type);
	static const EVertexElementType VertexElementType = VET_UShort2;
	static const EPixelFormat Format = PF_R32_UINT;
};

struct FHairStrandsInterpolation0Format
{
	typedef FHairInterpolation0Vertex Type;
	typedef FHairInterpolation0Vertex::BulkType BulkType;

	static const uint32 ComponentCount = 1;
	static const uint32 SizeInByte = sizeof(Type);
	static const EVertexElementType VertexElementType = VET_UShort4;
	static const EPixelFormat Format = PF_R16G16B16A16_UINT;
};

struct FHairStrandsInterpolation1Format
{
	typedef FHairInterpolation1Vertex Type;
	typedef FHairInterpolation1Vertex::BulkType BulkType;

	static const uint32 ComponentCount = 1;
	static const uint32 SizeInByte = sizeof(Type);
	static const EVertexElementType VertexElementType = VET_UShort4;
	static const EPixelFormat Format = PF_R16G16B16A16_UINT;
};

struct FHairStrandsRootIndexFormat
{
	typedef uint32 Type;
	typedef uint32 BulkType;

	static const uint32 ComponentCount = 1;
	static const uint32 SizeInByte = sizeof(Type);
	static const EVertexElementType VertexElementType = VET_UInt;
	static const EPixelFormat Format = PF_R32_UINT;
};

struct FHairStrandsRaytracingFormat
{
	typedef FVector4f Type;

	static const uint32 ComponentCount = 1;
	static const uint32 SizeInByte = sizeof(Type);
	static const EVertexElementType VertexElementType = VET_Float4;
	static const EPixelFormat Format = PF_A32B32G32R32F;
};

/** Hair strands index format */
struct HAIRSTRANDSCORE_API FHairStrandsIndexFormat
{
	using Type = uint32;
	using BulkType = uint32;
	static const uint32 ComponentCount = 1;
	static const uint32 SizeInByte = sizeof(Type);
	static const EVertexElementType VertexElementType = VET_UInt;
	static const EPixelFormat Format = PF_R32_UINT;
};

/** Hair strands weights format */
struct FHairStrandsWeightFormat
{
	using Type = float;
	using BulkType = float;
	static const uint32 ComponentCount = 1;
	static const uint32 SizeInByte = sizeof(Type);
	static const EVertexElementType VertexElementType = VET_Float1;
	static const EPixelFormat Format = PF_R32_FLOAT;
};

/** 
 * Skinned mesh triangle vertex position format
 */
struct FHairStrandsMeshTrianglePositionFormat
{
	using Type = FVector4f;
	using BulkType = FVector4f;
	static const uint32 ComponentCount = 1;
	static const uint32 SizeInByte = sizeof(Type);
	static const EVertexElementType VertexElementType = VET_Float4;
	static const EPixelFormat Format = PF_A32B32G32R32F;
};

// Encode Section ID and triangle Index from the source skel. mesh
struct FHairStrandsUniqueTriangleIndexFormat
{
	using Type = uint32;
	using BulkType = uint32;
	static const uint32 ComponentCount = 1;
	static const uint32 SizeInByte = sizeof(Type);
	static const EVertexElementType VertexElementType = VET_UInt;
	static const EPixelFormat Format = PF_R32_UINT;
};

struct FHairStrandsRootToUniqueTriangleIndexFormat
{
	using Type = uint32;
	using BulkType = uint32;
	static const uint32 ComponentCount = 1;
	static const uint32 SizeInByte = sizeof(Type);
	static const EVertexElementType VertexElementType = VET_UInt;
	static const EPixelFormat Format = PF_R32_UINT;
};

struct FHairStrandsRootBarycentricFormat
{
	using Type = uint32;
	using BulkType = uint32;
	static const uint32 ComponentCount = 1;
	static const uint32 SizeInByte = sizeof(Type);
	static const EVertexElementType VertexElementType = VET_UInt;
	static const EPixelFormat Format = PF_R32_UINT;
};

struct FHairStrandsRootUtils
{
	static uint32	 EncodeTriangleIndex(uint32 TriangleIndex, uint32 SectionIndex);
	static void		 DecodeTriangleIndex(uint32 Encoded, uint32& OutTriangleIndex, uint32& OutSectionIndex);
	static uint32	 EncodeBarycentrics(const FVector2f& B);
	static FVector2f DecodeBarycentrics(uint32 B);
	static uint32	 PackUVs(const FVector2f& UV);
	static float	 PackUVsToFloat(const FVector2f& UV);
};

struct FHairStrandsInterpolationBulkData;

/** Hair strands points interpolation attributes */
struct HAIRSTRANDSCORE_API FHairStrandsInterpolationDatas
{
	/** Set the number of interpolated points */
	void SetNum(const uint32 NumPoints);

	/** Reset the interpolated points to 0 */
	void Reset();

	/** Get the number of interpolated points */
	uint32 Num() const { return PointsSimCurvesVertexIndex.Num(); }

	bool IsValid() const { return PointsSimCurvesIndex.Num() > 0; }

	/** Simulation curve indices, ordered by closest influence */
	TArray<FIntVector> PointsSimCurvesIndex;

	/** Closest vertex indices on simulation curve, ordered by closest influence */
	TArray<FIntVector> PointsSimCurvesVertexIndex;

	/** Lerp value between the closest vertex indices and the next one, ordered by closest influence */
	TArray<FVector3f> PointsSimCurvesVertexLerp;

	/** Weight of vertex indices on simulation curve, ordered by closest influence */
	TArray<FVector3f>	PointsSimCurvesVertexWeights;

	/** True, if interpolation data are built using a single guide */
	bool bUseUniqueGuide = false;
};

struct HAIRSTRANDSCORE_API FHairStrandsInterpolationBulkData
{
	enum EDataFlags
	{
		DataFlags_HasData = 1,
		DataFlags_HasSingleGuideData = 2,
	};

	void Reset();
	void Serialize(FArchive& Ar, UObject* Owner);
	uint32 GetPointCount() const { return PointCount; };

	uint32 Flags = 0;
	uint32 PointCount = 0;
	uint32 SimPointCount = 0;
	FByteBulkData Interpolation;	// FHairStrandsInterpolationFormat  - Per-rendering-vertex interpolation data (closest guides, weight factors, ...). Data for a single guide
	FByteBulkData Interpolation0;	// FHairStrandsInterpolation0Format - Per-rendering-vertex interpolation data (closest guides, weight factors, ...). Data for up to 3 guides
	FByteBulkData Interpolation1;	// FHairStrandsInterpolation1Format - Per-rendering-vertex interpolation data (closest guides, weight factors, ...). Data for up to 3 guides
	FByteBulkData SimRootPointIndex;// FHairStrandsRootIndexFormat      - Per-rendering-vertex index of the sim-root vertex
};

/** Hair strands points attribute */
struct HAIRSTRANDSCORE_API FHairStrandsPoints
{
	/** Set the number of points */
	void SetNum(const uint32 NumPoints);

	/** Reset the points to 0 */
	void Reset();

	/** Get the number of points */
	uint32 Num() const { return PointsPosition.Num();  }

	/** Points position in local space */
	TArray<FVector3f> PointsPosition;

	/** Normalized radius relative to the max one */
	TArray<float> PointsRadius; // [0..1]

	/** Normalized length */
	TArray<float> PointsCoordU; // [0..1]

	/** Material base color */
	TArray<FLinearColor> PointsBaseColor; // [0..1]

	/** Material roughness */
	TArray<float> PointsRoughness; // [0..1]
};

/** Hair strands Curves attribute */
struct HAIRSTRANDSCORE_API FHairStrandsCurves
{
	/** Set the number of Curves */
	void SetNum(const uint32 NumPoints);

	/** Reset the curves to 0 */
	void Reset();

	/** Get the number of Curves */
	uint32 Num() const { return CurvesCount.Num(); }

	bool HasPrecomputedWeights() const { return CurvesClosestGuideIDs.Num() > 0 && CurvesClosestGuideWeights.Num() > 0; }

	/** Number of points per rod */
	TArray<uint16> CurvesCount;

	/** An offset represent the rod start in the point list */
	TArray<uint32> CurvesOffset;

	/** Normalized length relative to the max one */
	TArray<float> CurvesLength; // [0..1]

	/** Roots UV. Support UDIM coordinate up to 256x256 */
	TArray<FVector2f> CurvesRootUV; // [0..256]

	/** Strand ID associated with each curve */
	TArray<int> StrandIDs;

	/** Mapping of imported Groom ID to index */
	TMap<int, int> GroomIDToIndex;

	/** Custom guide IDs (indexed with StrandID) */
	TArray<FIntVector> CurvesClosestGuideIDs;

	/** Custom guid weights (indexed with StrandID) */
	TArray<FVector> CurvesClosestGuideWeights;

	/** Max strands Curves length */
	float MaxLength = 0;

	/** Max strands Curves radius */
	float MaxRadius = 0;
};

struct FHairStrandsBulkData;

/** Hair strands datas that are stored on CPU */
struct HAIRSTRANDSCORE_API FHairStrandsDatas
{
	/* Get the total number of points */
	uint32 GetNumPoints() const { return StrandsPoints.Num(); }

	/* Get the total number of Curves */
	uint32 GetNumCurves() const { return StrandsCurves.Num(); }

	void Reset();

	bool IsValid() const { return StrandsCurves.Num() > 0 && StrandsPoints.Num() > 0; }

	/** List of all the strands points */
	FHairStrandsPoints StrandsPoints;

	/** List of all the strands curves */
	FHairStrandsCurves StrandsCurves;

	/** The Standard Hair Density */
	float HairDensity = 1;

	/* Strands bounding box */
	FBox BoundingBox;
};

struct HAIRSTRANDSCORE_API FHairStrandsBulkData
{
	enum EDataFlags
	{
		DataFlags_HasData = 1,			// Contains valid data. Otherwise: Position, Attributes, ... are all empty
		DataFlags_HasMaterialData = 2	// Contains material data (albedo and/or roughness)
	};

	void Serialize(FArchive& Ar, UObject* Owner);

	bool IsValid() const { return CurveCount > 0 && PointCount > 0; }
	void Reset();

	uint32 GetNumCurves() const { return CurveCount;  };
	uint32 GetNumPoints() const { return PointCount; };
	float  GetMaxLength() const	{ return MaxLength; };
	float  GetMaxRadius() const { return MaxRadius; }
	FVector GetPositionOffset() const { return BoundingBox.GetCenter(); }
	const FBox& GetBounds() const { return BoundingBox; }

	uint32 CurveCount = 0;
	uint32 PointCount = 0;
	float MaxLength = 0;
	float MaxRadius = 0;
	FBox BoundingBox;
	uint32 Flags = 0;

	FByteBulkData Positions;	// Size = PointCount
	FByteBulkData Attributes0;	// Size = PointCount
	FByteBulkData Attributes1;	// Size = PointCount
	FByteBulkData Materials;	// Size = PointCount
	FByteBulkData CurveOffsets;	// Size = CurveCount+1 - Store the root point index for the curve
};

/** Hair strands debug data */
struct HAIRSTRANDSCORE_API FHairStrandsDebugDatas
{
	bool IsValid() const { return VoxelData.Num() > 0;  }

	static const uint32 InvalidIndex = ~0u;
	struct FOffsetAndCount
	{
		uint32 Offset = 0u;
		uint32 Count = 0u;
	};

	struct FVoxel
	{
		uint32 Index0 = InvalidIndex;
		uint32 Index1 = InvalidIndex;
	};

	struct FDesc
	{
		FVector3f VoxelMinBound = FVector3f::ZeroVector;
		FVector3f VoxelMaxBound = FVector3f::ZeroVector;
		FIntVector VoxelResolution = FIntVector::ZeroValue;
		float VoxelSize = 0;
		uint32 MaxSegmentPerVoxel = 0;
	};

	FDesc VoxelDescription;
	TArray<FOffsetAndCount> VoxelOffsetAndCount;
	TArray<FVoxel> VoxelData;

	struct FResources
	{
		FDesc VoxelDescription;

		TRefCountPtr<FRDGPooledBuffer> VoxelOffsetAndCount;
		TRefCountPtr<FRDGPooledBuffer> VoxelData;
	};
};

/* Bulk data for root resources (GPU resources are stored into FHairStrandsRootResources) */
struct FHairStrandsRootBulkData
{
	FHairStrandsRootBulkData();
	void Serialize(FArchive& Ar, UObject* Owner);
	void Reset();
	bool HasProjectionData() const;
	bool IsValid() const { return RootCount > 0; }
	const TArray<uint32>& GetValidSectionIndices(int32 LODIndex) const;

	uint32 GetDataSize() const
	{
		uint32 Total = 0;
		Total += VertexToCurveIndexBuffer.IsBulkDataLoaded() ? VertexToCurveIndexBuffer.GetBulkDataSize() : 0u;
		for (const FMeshProjectionLOD& LOD : MeshProjectionLODs)
		{
			Total += LOD.UniqueTriangleIndexBuffer.IsBulkDataLoaded() ?			LOD.UniqueTriangleIndexBuffer.GetBulkDataSize() : 0u;
			Total += LOD.RootToUniqueTriangleIndexBuffer.IsBulkDataLoaded() ?	LOD.RootToUniqueTriangleIndexBuffer.GetBulkDataSize() : 0u;
			Total += LOD.RootBarycentricBuffer.IsBulkDataLoaded() ?				LOD.RootBarycentricBuffer.GetBulkDataSize() : 0u;
			Total += LOD.RestUniqueTrianglePosition0Buffer.IsBulkDataLoaded() ?	LOD.RestUniqueTrianglePosition0Buffer.GetBulkDataSize() : 0u;
			Total += LOD.RestUniqueTrianglePosition1Buffer.IsBulkDataLoaded() ?	LOD.RestUniqueTrianglePosition1Buffer.GetBulkDataSize() : 0u;
			Total += LOD.RestUniqueTrianglePosition2Buffer.IsBulkDataLoaded() ?	LOD.RestUniqueTrianglePosition2Buffer.GetBulkDataSize() : 0u;
			Total += LOD.MeshInterpolationWeightsBuffer.IsBulkDataLoaded() ?	LOD.MeshInterpolationWeightsBuffer.GetBulkDataSize() : 0u;
			Total += LOD.MeshSampleIndicesBuffer.IsBulkDataLoaded() ?			LOD.MeshSampleIndicesBuffer.GetBulkDataSize() : 0u;
			Total += LOD.RestSamplePositionsBuffer.IsBulkDataLoaded() ?			LOD.RestSamplePositionsBuffer.GetBulkDataSize() : 0u;
			Total += LOD.UniqueSectionIndices.GetAllocatedSize();
		}
		return Total;
	}

	struct FMeshProjectionLOD
	{
		int32 LODIndex = -1;
		uint32 UniqueTriangleCount = 0;

		/* Map each root onto the unique triangle Id (per-root) */
		FByteBulkData RootToUniqueTriangleIndexBuffer;
		/* Root's barycentric (per-root) */
		FByteBulkData RootBarycentricBuffer;

		/* Unique triangles list from skeleton mesh section IDs and triangle IDs (per-unique-triangle) */
		FByteBulkData UniqueTriangleIndexBuffer;

		/* Rest triangle positions (per-unique-triangle) */
		FByteBulkData RestUniqueTrianglePosition0Buffer;
		FByteBulkData RestUniqueTrianglePosition1Buffer;
		FByteBulkData RestUniqueTrianglePosition2Buffer;

		/* Number of samples used for the mesh interpolation */
		uint32 SampleCount = 0;

		/* Store the hair interpolation weights | Size = SamplesCount * SamplesCount (per-sample)*/
		FByteBulkData MeshInterpolationWeightsBuffer;

		/* Store the samples vertex indices (per-sample) */
		FByteBulkData MeshSampleIndicesBuffer;

		/* Store the samples rest positions (per-sample) */
		FByteBulkData RestSamplePositionsBuffer;

		/* Store the mesh section indices which are relevant for this root LOD data */
		TArray<uint32> UniqueSectionIndices;
	};

	/* Number of roots */
	uint32 RootCount = 0;

	/* Number of control points */
	uint32 PointCount = 0;

	/* Curve index for every vertices */
	FByteBulkData VertexToCurveIndexBuffer;

	/* Store the hair projection information for each mesh LOD */
	TArray<FMeshProjectionLOD> MeshProjectionLODs;
};

/* Source data for building root bulk data */
struct FHairStrandsRootData
{
	FHairStrandsRootData();
	void Reset();
	bool HasProjectionData() const;
	bool IsValid() const { return RootCount > 0; }

	struct FMeshProjectionLOD
	{
		int32 LODIndex = -1;

		/* Triangle on which a root is attached */
		/* When the projection is done with source to target mesh transfer, the projection indices does not match.
		   In this case we need to separate index computation. The barycentric coords remain the same however. */
		TArray<FHairStrandsRootToUniqueTriangleIndexFormat::Type> RootToUniqueTriangleIndexBuffer;
		TArray<FHairStrandsRootBarycentricFormat::Type> RootBarycentricBuffer;

		/* Strand hair roots translation and rotation in rest position relative to the bound triangle. Positions are relative to the rest root center */
		TArray<FHairStrandsUniqueTriangleIndexFormat::Type> UniqueTriangleIndexBuffer;
		TArray<FHairStrandsMeshTrianglePositionFormat::Type> RestUniqueTrianglePosition0Buffer;
		TArray<FHairStrandsMeshTrianglePositionFormat::Type> RestUniqueTrianglePosition1Buffer;
		TArray<FHairStrandsMeshTrianglePositionFormat::Type> RestUniqueTrianglePosition2Buffer;

		/* Number of samples used for the mesh interpolation */
		uint32 SampleCount = 0;

		/* Store the hair interpolation weights | Size = SamplesCount * SamplesCount */
		TArray<FHairStrandsWeightFormat::Type> MeshInterpolationWeightsBuffer;

		/* Store the samples vertex indices */
		TArray<FHairStrandsIndexFormat::Type> MeshSampleIndicesBuffer;

		/* Store the samples rest positions */
		TArray<FHairStrandsMeshTrianglePositionFormat::Type> RestSamplePositionsBuffer;

		/* Store the mesh section indices which are relevant for this root LOD data */
		TArray<uint32> UniqueSectionIds;
	};

	/* Number of roots */
	uint32 RootCount = 0;

	/* Number of control points */
	uint32 PointCount = 0;

	/* Curve index for every vertices */
	TArray<FHairStrandsIndexFormat::Type> VertexToCurveIndexBuffer;

	/* Store the hair projection information for each mesh LOD */
	TArray<FMeshProjectionLOD> MeshProjectionLODs;
};

/* Structure describing the LOD settings (Screen size, vertex info, ...) for each clusters.
	The packed version of this structure corresponds to the GPU data layout (HairStrandsClusterCommon.ush)
	This uses by the GPU LOD selection. */
struct FHairClusterInfo
{
	static const uint32 MaxLOD = 8;

	struct Packed
	{
		uint32 LODInfoOffset : 24;
		uint32 LODCount : 8;

		uint32 LOD_ScreenSize_0 : 10;
		uint32 LOD_ScreenSize_1 : 10;
		uint32 LOD_ScreenSize_2 : 10;
		uint32 Pad0 : 2;

		uint32 LOD_ScreenSize_3 : 10;
		uint32 LOD_ScreenSize_4 : 10;
		uint32 LOD_ScreenSize_5 : 10;
		uint32 Pad1 : 2;

		uint32 LOD_ScreenSize_6 : 10;
		uint32 LOD_ScreenSize_7 : 10;
		uint32 LOD_bIsVisible : 8;
		uint32 Pad2 : 4;
	};
	typedef FUintVector4 BulkType;

	FHairClusterInfo()
	{
		for (uint32 LODIt = 0; LODIt < MaxLOD; ++LODIt)
		{
			ScreenSize[LODIt] = 0;
			bIsVisible[LODIt] = true;
		}
	}

	uint32 LODCount = 0;
	uint32 LODInfoOffset = 0;
	TStaticArray<float,MaxLOD> ScreenSize;
	TStaticArray<bool, MaxLOD> bIsVisible;
};

/* Structure describing the LOD settings common to all clusters. The layout of this structure is
	identical the GPU data layout (HairStrandsClusterCommon.ush). This uses by the GPU LOD selection. */
struct FHairClusterLODInfo
{
	uint32 VertexOffset = 0;
	uint32 VertexCount0 = 0;
	uint32 VertexCount1 = 0;
	FFloat16 RadiusScale0 = 0;
	FFloat16 RadiusScale1 = 0;
};

struct FHairClusterInfoFormat
{
	typedef FHairClusterInfo::Packed Type;
	typedef FHairClusterInfo::Packed BulkType;
	static const uint32 SizeInByte = sizeof(Type);
};

struct FHairClusterLODInfoFormat
{
	typedef FHairClusterLODInfo Type;
	typedef FHairClusterLODInfo BulkType;
	static const uint32 SizeInByte = sizeof(Type);
};

struct FHairClusterIndexFormat
{
	typedef uint32 Type;
	typedef uint32 BulkType;
	static const uint32 SizeInByte = sizeof(Type);
	static const EPixelFormat Format = PF_R32_UINT;
};

struct HAIRSTRANDSCORE_API FHairStrandsClusterCullingData
{
	FHairStrandsClusterCullingData();
	void Reset();
	bool IsValid() const { return ClusterCount > 0 && VertexCount > 0; }

	/* Set LOD visibility, allowing to remove the simulation/rendering of certain LOD */
	TArray<bool>				LODVisibility;

	/* Screen size at which LOD should switches on CPU */
	TArray<float>				CPULODScreenSize;

	/* LOD info for the various clusters for LOD management on GPU */
	TArray<FHairClusterInfo>	ClusterInfos;
	TArray<FHairClusterLODInfo> ClusterLODInfos;
	TArray<uint32>				VertexToClusterIds;
	TArray<uint32>				ClusterVertexIds;

	/* Number of cluster  */
	uint32 ClusterCount = 0;

	/* Number of vertex  */
	uint32 VertexCount = 0;
};

struct HAIRSTRANDSCORE_API FHairStrandsClusterCullingBulkData
{
	FHairStrandsClusterCullingBulkData();
	void Reset();
	void Serialize(FArchive& Ar, UObject* Owner);
	bool IsValid() const { return ClusterCount > 0 && VertexCount > 0; }
	void Validate(bool bIsSaving);

	/* Set LOD visibility, allowing to remove the simulation/rendering of certain LOD */
	TArray<bool>	LODVisibility;

	/* Screen size at which LOD should switches on CPU */
	TArray<float>	CPULODScreenSize;

	/* LOD info for the various clusters for LOD management on GPU */
	FByteBulkData	PackedClusterInfos;		// Size - ClusterCount
	FByteBulkData	ClusterLODInfos;		// Size - ClusterLODCount
	FByteBulkData	VertexToClusterIds;		// Size - VertexCount
	FByteBulkData	ClusterVertexIds;		// Size - VertexLODCount

	uint32 ClusterCount = 0;
	uint32 ClusterLODCount = 0;
	uint32 VertexCount = 0;
	uint32 VertexLODCount = 0;
};

