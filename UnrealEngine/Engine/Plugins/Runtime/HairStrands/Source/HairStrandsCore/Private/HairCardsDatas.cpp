// Copyright Epic Games, Inc. All Rights Reserved.

#include "HairCardsDatas.h"

// Serialization code for cards structures
// Note that if there are changes in the serialized structures,
// including the types used in them such as the RenderData structures,
// CustomVersion will be required to handle the changes

void FHairCardsBulkData::Serialize(FArchive& Ar)
{
	Ar << Positions;
	Ar << Normals;
	Ar << UVs;
	Ar << Materials;
	Ar << Indices;
	Ar << BoundingBox;
}

FArchive& operator<<(FArchive& Ar, FHairCardsInterpolationVertex& CardInterpVertex)
{
	if (Ar.IsLoading())
	{
		uint32 Value;
		Ar << Value;
		CardInterpVertex.VertexIndex = Value & 0x00FFFFFF; // first 24 bits
		CardInterpVertex.VertexLerp = Value >> 24;
	}
	else
	{
		uint32 Value = CardInterpVertex.VertexIndex | (CardInterpVertex.VertexLerp << 24);
		Ar << Value;
	}

	return Ar;
}

void FHairMeshesBulkData::Serialize(FArchive& Ar)
{
	Ar << Positions;
	Ar << Normals;
	Ar << UVs;
	Ar << Indices;
	Ar << BoundingBox;
}


/////////////////////////////////////////////////////////////////////////////////////////
void FHairCardsInterpolationDatas::SetNum(const uint32 NumPoints)
{
	PointsSimCurvesIndex.SetNum(NumPoints);
	PointsSimCurvesVertexIndex.SetNum(NumPoints);
	PointsSimCurvesVertexLerp.SetNum(NumPoints);
}

void FHairCardsInterpolationDatas::Reset()
{
	PointsSimCurvesIndex.Empty();
	PointsSimCurvesVertexIndex.Empty();
	PointsSimCurvesVertexLerp.Empty();
}

void FHairCardsInterpolationBulkData::Serialize(FArchive& Ar)
{
	Ar << Interpolation;
}