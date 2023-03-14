// Copyright Epic Games, Inc. All Rights Reserved.

#include "HairStrandsDatas.h"
#include "UObject/ReleaseObjectVersion.h"
#include "UObject/UE5ReleaseStreamObjectVersion.h"

void FHairStrandsInterpolationDatas::SetNum(const uint32 NumCurves)
{
	PointsSimCurvesVertexWeights.SetNum(NumCurves);
	PointsSimCurvesVertexLerp.SetNum(NumCurves);
	PointsSimCurvesVertexIndex.SetNum(NumCurves);
	PointsSimCurvesIndex.SetNum(NumCurves);
}

void FHairStrandsCurves::SetNum(const uint32 NumCurves)
{
	CurvesOffset.SetNum(NumCurves + 1);
	CurvesCount.SetNum(NumCurves);
	CurvesLength.SetNum(NumCurves);
	CurvesRootUV.SetNum(NumCurves);

	// Curves clossest guide are not initialized to track if the curves 
	// data contains or not precomputed weights
	// CurvesClosestGuideIDs.SetNum(0);
	// CurvesClosestGuideWeights.SetNum(0);
}

void FHairStrandsPoints::SetNum(const uint32 NumPoints)
{
	PointsPosition.SetNum(NumPoints);
	PointsRadius.SetNum(NumPoints);
	PointsCoordU.SetNum(NumPoints);
	PointsBaseColor.SetNum(NumPoints);
	PointsRoughness.SetNum(NumPoints);
}

void FHairStrandsInterpolationDatas::Reset()
{
	PointsSimCurvesVertexWeights.Reset();
	PointsSimCurvesVertexLerp.Reset();
	PointsSimCurvesVertexIndex.Reset();
	PointsSimCurvesIndex.Reset();
}

void FHairStrandsCurves::Reset()
{
	CurvesOffset.Reset();
	CurvesCount.Reset();
	CurvesLength.Reset();
	CurvesRootUV.Reset();
	CurvesClosestGuideIDs.Reset();
	CurvesClosestGuideWeights.Reset();
}

void FHairStrandsPoints::Reset()
{
	PointsPosition.Reset();
	PointsRadius.Reset();
	PointsCoordU.Reset();
}

FArchive& operator<<(FArchive& Ar, FVector4_16& Vertex)
{
	Ar << Vertex.X;
	Ar << Vertex.Y;
	Ar << Vertex.Z;
	Ar << Vertex.W;
	return Ar;
}

FArchive& operator<<(FArchive& Ar, FPackedHairVertex& Vertex)
{
	Ar << Vertex.X;
	Ar << Vertex.Y;
	Ar << Vertex.Z;
	Ar << Vertex.PackedRadiusAndType;
	Ar << Vertex.UCoord;

	return Ar;
}

FArchive& operator<<(FArchive& Ar, FHairMaterialVertex& Vertex)
{
	Ar << Vertex.BaseColorR;
	Ar << Vertex.BaseColorG;
	Ar << Vertex.BaseColorB;
	Ar << Vertex.Roughness;

	return Ar;
}

FArchive& operator<<(FArchive& Ar, FHairInterpolation0Vertex& Vertex)
{
	Ar << Vertex.Index0;
	Ar << Vertex.Index1;
	Ar << Vertex.Index2;
	Ar << Vertex.VertexWeight0;
	Ar << Vertex.VertexWeight1;

	return Ar;
}

FArchive& operator<<(FArchive& Ar, FHairInterpolationVertex& Vertex)
{
	Ar << Vertex.VertexGuideIndex0;
	Ar << Vertex.VertexGuideIndex1;
	Ar << Vertex.VertexLerp;

	return Ar;
}

FArchive& operator<<(FArchive& Ar, FHairInterpolation1Vertex& Vertex)
{
	Ar.UsingCustomVersion(FReleaseObjectVersion::GUID);

	if (Ar.CustomVer(FReleaseObjectVersion::GUID) >= FReleaseObjectVersion::GroomAssetVersion3)
	{
		Ar << Vertex.VertexIndex0;
		Ar << Vertex.VertexIndex1;
		Ar << Vertex.VertexIndex2;

		Ar << Vertex.VertexLerp0;
		Ar << Vertex.VertexLerp1;
		Ar << Vertex.VertexLerp2;

		Ar << Vertex.Pad0;
		Ar << Vertex.Pad1;
	}
	else
	{
		Ar << Vertex.VertexIndex0;
		Ar << Vertex.VertexIndex1;
		Ar << Vertex.VertexIndex2;

		uint8 Pad0 = 0;
		Ar << Pad0;

		if (Ar.IsLoading())
		{
			Vertex.VertexLerp0 = 0;
			Vertex.VertexLerp1 = 0;
			Vertex.VertexLerp2 = 0;
		}
	}

	return Ar;
}

void FHairStrandsInterpolationBulkData::Reset()
{
	Flags = 0;
	PointCount = 0;
	SimPointCount = 0;
	
	// Deallocate memory if needed
	Interpolation.RemoveBulkData();
	Interpolation0.RemoveBulkData();
	Interpolation1.RemoveBulkData();
	SimRootPointIndex.RemoveBulkData();

	// Reset the bulk byte buffer to ensure the (serialize) data size is reset to 0
	Interpolation		= FByteBulkData();
	Interpolation0		= FByteBulkData();
	Interpolation1		= FByteBulkData();
	SimRootPointIndex	= FByteBulkData();
}

void FHairStrandsInterpolationBulkData::Serialize(FArchive& Ar, UObject* Owner)
{
	static_assert(sizeof(FHairInterpolationVertex::BulkType) == sizeof(FHairInterpolationVertex));
	static_assert(sizeof(FHairInterpolation0Vertex::BulkType) == sizeof(FHairInterpolation0Vertex));
	static_assert(sizeof(FHairInterpolation1Vertex::BulkType) == sizeof(FHairInterpolation1Vertex));
	static_assert(sizeof(FHairStrandsRootIndexFormat::BulkType) == sizeof(FHairStrandsRootIndexFormat::Type));

	if (Ar.IsSaving())
	{
		const uint32 BulkFlags = BULKDATA_Force_NOT_InlinePayload;
		Interpolation.SetBulkDataFlags(BulkFlags);
		Interpolation0.SetBulkDataFlags(BulkFlags);
		Interpolation1.SetBulkDataFlags(BulkFlags);
		SimRootPointIndex.SetBulkDataFlags(BulkFlags);
	}

	Ar << Flags;
	Ar << PointCount;
	Ar << SimPointCount;

	if (!!(Flags & DataFlags_HasData))
	{
		const int32 ChunkIndex = 0;
		bool bAttemptFileMapping = false;

		if (!!(Flags & DataFlags_HasSingleGuideData))
		{
			Interpolation.Serialize(Ar, Owner, ChunkIndex, bAttemptFileMapping);
		}
		else
		{
			Interpolation0.Serialize(Ar, Owner, ChunkIndex, bAttemptFileMapping);
			Interpolation1.Serialize(Ar, Owner, ChunkIndex, bAttemptFileMapping);
		}
		SimRootPointIndex.Serialize(Ar, Owner, ChunkIndex, bAttemptFileMapping);
	}
}

void FHairStrandsBulkData::Serialize(FArchive& Ar, UObject* Owner)
{
	static_assert(sizeof(FHairStrandsPositionFormat::BulkType) == sizeof(FHairStrandsPositionFormat::Type));
	static_assert(sizeof(FHairStrandsAttribute0Format::BulkType) == sizeof(FHairStrandsAttribute0Format::Type));
	static_assert(sizeof(FHairStrandsAttribute1Format::BulkType) == sizeof(FHairStrandsAttribute1Format::Type));
	static_assert(sizeof(FHairStrandsMaterialFormat::BulkType) == sizeof(FHairStrandsMaterialFormat::Type));
	static_assert(sizeof(FHairStrandsRootIndexFormat::BulkType) == sizeof(FHairStrandsRootIndexFormat::Type)); 

	Ar.UsingCustomVersion(FReleaseObjectVersion::GUID);
	Ar.UsingCustomVersion(FUE5ReleaseStreamObjectVersion::GUID);

	Ar << CurveCount;
	Ar << PointCount;
	Ar << MaxLength;
	Ar << MaxRadius;
	Ar << BoundingBox;
	Ar << Flags;

	// Forced not inline means the bulk data won't automatically be loaded when we deserialize
	// but only when we explicitly take action to load it
	if (Ar.IsSaving())
	{
		const uint32 BulkFlags = BULKDATA_Force_NOT_InlinePayload;
		Positions.SetBulkDataFlags(BulkFlags);
		Attributes0.SetBulkDataFlags(BulkFlags);
		Attributes1.SetBulkDataFlags(BulkFlags);
		Materials.SetBulkDataFlags(BulkFlags);
		CurveOffsets.SetBulkDataFlags(BulkFlags);
	}

	if (!!(Flags & DataFlags_HasData))
	{
		const int32 ChunkIndex = 0;
		bool bAttemptFileMapping = false;

		Positions.Serialize(Ar, Owner, ChunkIndex, bAttemptFileMapping);
		Attributes0.Serialize(Ar, Owner, ChunkIndex, bAttemptFileMapping);
		Attributes1.Serialize(Ar, Owner, ChunkIndex, bAttemptFileMapping);

		if (!!(Flags & DataFlags_HasMaterialData))
		{
			Materials.Serialize(Ar, Owner, ChunkIndex, bAttemptFileMapping);
		}
		CurveOffsets.Serialize(Ar, Owner, ChunkIndex, bAttemptFileMapping);
	}
}

void FHairStrandsBulkData::Reset()
{
	CurveCount = 0;
	PointCount = 0;
	MaxLength = 0;
	MaxRadius = 0;
	BoundingBox = FBox(EForceInit::ForceInit);
	Flags = 0;

	// Deallocate memory if needed
	Positions.RemoveBulkData();
	Attributes0.RemoveBulkData();
	Attributes1.RemoveBulkData();
	Materials.RemoveBulkData();
	CurveOffsets.RemoveBulkData();

	// Reset the bulk byte buffer to ensure the (serialize) data size is reset to 0
	Positions 	 = FByteBulkData();
	Attributes0  = FByteBulkData();
	Attributes1  = FByteBulkData();
	Materials 	 = FByteBulkData();
	CurveOffsets = FByteBulkData();
}

void FHairStrandsDatas::Reset()
{
	StrandsCurves.Reset();
	StrandsPoints.Reset();
}
