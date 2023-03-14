// Copyright Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterWarpBlendLoader_WarpMap.h"
#include "RHI.h"
#include "DisplayClusterShadersLog.h"
#include "RHI.h"

THIRD_PARTY_INCLUDES_START
#include "mpcdiProfile.h"
#include "mpcdiReader.h"
#include "mpcdiDisplay.h"
#include "mpcdiBuffer.h"
#include "mpcdiRegion.h"
#include "mpcdiAlphaMap.h"
#include "mpcdiBetaMap.h"
#include "mpcdiDistortionMap.h"
#include "mpcdiGeometryWarpFile.h"
THIRD_PARTY_INCLUDES_END


static constexpr float kEpsilon = 0.00001f;

namespace
{
	float GetUnitToCentimeter(mpcdi::GeometricUnit GeomUnit)
	{
		switch (GeomUnit)
		{
		case mpcdi::GeometricUnitmm:
			return 1.f / 10.f;
		case mpcdi::GeometricUnitcm:
			return 1.f;
		case mpcdi::GeometricUnitdm:
			return 10.f;
		case mpcdi::GeometricUnitm:
			return 100.f;
		case mpcdi::GeometricUnitin:
			return 2.54f;
		case mpcdi::GeometricUnitft:
			return 30.48f;
		case mpcdi::GeometricUnityd:
			return 91.44f;
		case mpcdi::GeometricUnitunkown:
			return 1.f;
		default:
			check(false);
			return 1.f;
		}
	}
}


bool FDisplayClusterWarpBlendLoader_WarpMap::Load(FLoadedWarpMapData& OutWarpMapData, EDisplayClusterWarpProfileType ProfileType, mpcdi::GeometryWarpFile* SourceWarpMap)
{
	check(SourceWarpMap);

	if (!OutWarpMapData.Initialize(SourceWarpMap->GetSizeX(), SourceWarpMap->GetSizeY()))
	{
		return false;
	}

	bool bIsProfile2D = true;
	float UnitToCentemeter = 1.f;

	FMatrix ConventionMatrix = FMatrix::Identity;

	switch (ProfileType)
	{
	case EDisplayClusterWarpProfileType::warp_A3D:
		// Unreal is in cm, so we need to convert to cm.
		UnitToCentemeter = GetUnitToCentimeter(SourceWarpMap->GetGeometricUnit());

		// Convert from MPCDI convention to Unreal convention
		// MPCDI is Right Handed (Y is up, X is left, Z is in the screen)
		// Unreal is Left Handed (Z is up, X in the screen, Y is right)
		ConventionMatrix = FMatrix(
			FPlane(0.f, UnitToCentemeter, 0.f, 0.f),
			FPlane(0.f, 0.f, UnitToCentemeter, 0.f),
			FPlane(-UnitToCentemeter, 0.f, 0.f, 0.f),
			FPlane(0.f, 0.f, 0.f, 1.f));

		bIsProfile2D = false;

		break;

	default:
		break;
	};


	for (int32 WarpMapY = 0; WarpMapY < OutWarpMapData.Height; ++WarpMapY)
	{
		for (int32 WarpMapX = 0; WarpMapX < OutWarpMapData.Width; ++WarpMapX)
		{
			mpcdi::NODE& node = (*SourceWarpMap)(WarpMapX, WarpMapY);
			FVector t(node.r, node.g, bIsProfile2D ? 0.f : node.b);

			FVector4f& OutPts = OutWarpMapData.GetWarpDataPointRef(WarpMapX, WarpMapY);

			if ((!(FMath::Abs(t.X) < kEpsilon && FMath::Abs(t.Y) < kEpsilon && FMath::Abs(t.Z) < kEpsilon))
				&& (!FMath::IsNaN(t.X) && !FMath::IsNaN(t.Y) && !FMath::IsNaN(t.Z)))
			{
				const FVector ScaledPts = ConventionMatrix.TransformPosition(t);

				OutPts.X = ScaledPts.X;
				OutPts.Y = ScaledPts.Y;
				OutPts.Z = ScaledPts.Z;
				OutPts.W = 1;
			}
			else
			{
				OutPts = FVector4f(0.f, 0.f, 0.f, -1.f);
			}
		}
	}

	if (ProfileType == EDisplayClusterWarpProfileType::warp_A3D)
	{
		// Remove noise from warp mesh (small areas less than 3*3 quads)
		OutWarpMapData.ClearNoise(FIntPoint(3, 3), FIntPoint(2, 3));
	}

	return true;
}

bool FDisplayClusterWarpBlendLoader_WarpMap::Load(FLoadedWarpMapData& OutWarpMapData, EDisplayClusterWarpProfileType InProfileType, const TArray<FVector>& InPoints, int32 WarpX, int32 WarpY, float WorldScale, bool bIsUnrealGameSpace)
{
	if (!OutWarpMapData.Initialize(WarpX, WarpY))
	{
		return false;
	}

	OutWarpMapData.LoadGeometry(InProfileType, InPoints, WorldScale, bIsUnrealGameSpace);
	return true;
}

bool FDisplayClusterWarpBlendLoader_WarpMap::Load(FLoadedWarpMapData& OutWarpMapData, EDisplayClusterWarpProfileType InProfileType, mpcdi::PFM* SourcePFM, float PFMScale, bool bIsUnrealGameSpace)
{
	check(SourcePFM);

	if (!OutWarpMapData.Initialize(SourcePFM->GetSizeX(), SourcePFM->GetSizeY()))
	{
		return false;
	}

	TArray<FVector> WarpMeshPoints;
	WarpMeshPoints.Reserve(OutWarpMapData.Width * OutWarpMapData.Height);

	for (int32 WarpMapY = 0; WarpMapY < OutWarpMapData.Height; ++WarpMapY)
	{
		for (int32 WarpMapX = 0; WarpMapX < OutWarpMapData.Width; ++WarpMapX)
		{
			mpcdi::NODE node = SourcePFM->operator()(WarpMapX, WarpMapY);
			FVector pts(node.r, node.g, node.b);
			WarpMeshPoints.Add(pts);
		}
	}

	OutWarpMapData.LoadGeometry(InProfileType, WarpMeshPoints, PFMScale, bIsUnrealGameSpace);
	return true;
}

bool FLoadedWarpMapData::Initialize(int32 InWidth, int32 InHeight)
{
	if (FMath::Min(InWidth, InHeight) <= 1 || FMath::Max(InWidth, InHeight) >= GMaxTextureDimensions)
	{
		UE_LOG(LogDisplayClusterWarpBlend, Error, TEXT("Invalid PFM warpmap data size '%d x %d'"), InWidth, InHeight);

		return false;
	}

	Width = InWidth;
	Height = InHeight;
	WarpData = new FVector4f[Width * Height];

	return true;
}

FLoadedWarpMapData::~FLoadedWarpMapData()
{
	if (WarpData != nullptr)
	{
		delete WarpData;
		WarpData = nullptr;
	}
}

void FLoadedWarpMapData::LoadGeometry(EDisplayClusterWarpProfileType ProfileType, const TArray<FVector>& InPoints, float WorldScale, bool bIsUnrealGameSpace)
{
	check(InPoints.Num() == (Width * Height));

	FMatrix ConventionMatrix = FMatrix::Identity;
	switch (ProfileType)
	{
	case EDisplayClusterWarpProfileType::warp_A3D:
		if (bIsUnrealGameSpace)
		{
			ConventionMatrix = FMatrix(
				FPlane(WorldScale, 0.f, 0.f, 0.f),
				FPlane(0.f, WorldScale, 0.f, 0.f),
				FPlane(0.f, 0.f, WorldScale, 0.f),
				FPlane(0.f, 0.f, 0.f, 1.f));
		}
		else
		{
			// Convert from MPCDI convention to Unreal convention
			// MPCDI is Right Handed (Y is up, X is left, Z is in the screen)
			// Unreal is Left Handed (Z is up, X in the screen, Y is right)
			ConventionMatrix = FMatrix(
				FPlane(0.f, WorldScale, 0.f, 0.f),
				FPlane(0.f, 0.f, WorldScale, 0.f),
				FPlane(-WorldScale, 0.f, 0.f, 0.f),
				FPlane(0.f, 0.f, 0.f, 1.f));
		}
		break;

	default:
		break;
	};


	FVector4f* WarpDataPointIt = WarpData;
	for (const FVector& PointIt : InPoints)
	{
		const FVector& t = PointIt;

		FVector4f& OutPts = *WarpDataPointIt;
		WarpDataPointIt++;

		if ((!(FMath::Abs(t.X) < kEpsilon && FMath::Abs(t.Y) < kEpsilon && FMath::Abs(t.Z) < kEpsilon))
			&& (!FMath::IsNaN(t.X) && !FMath::IsNaN(t.Y) && !FMath::IsNaN(t.Z)))
		{
			const FVector ScaledPts = ConventionMatrix.TransformPosition(t);

			OutPts.X = ScaledPts.X;
			OutPts.Y = ScaledPts.Y;
			OutPts.Z = ScaledPts.Z;
			OutPts.W = 1;
		}
		else
		{
			OutPts = FVector4f(0.f, 0.f, 0.f, -1.f);
		}
	}
}



bool FLoadedWarpMapData::Is3DPointValid(int32 X, int32 Y) const
{
	if (WarpData && X >= 0 && X < Width && Y >= 0 && Y < Height)
	{
		const FVector4f& Point = GetWarpDataPointConstRef(X, Y);
		return Point.W > 0;
	}

	return false;
}

void FLoadedWarpMapData::ClearNoise(const FIntPoint& SearchXYDepth, const FIntPoint& AllowedXYDepthRules)
{
	if (Width > 10 && Height > 10)
	{
		//Remove noise for large warp mesh
		int32 MaxLoops = 50;
		while (MaxLoops-- > 0)
		{
			if (!RemoveDetachedPoints(SearchXYDepth, AllowedXYDepthRules))
			{
				break;
			}
		}
	}
}

int32 FLoadedWarpMapData::RemoveDetachedPoints(const FIntPoint& SearchLen, const FIntPoint& RemoveRule)
{
	const int32 SearchX = SearchLen.X * Width / 100;
	const int32 SearchY = SearchLen.Y * Height / 100;
	const int32 Rule1X = RemoveRule.X * Width / 100;
	const int32 Rule1Y = RemoveRule.Y * Height / 100;

	int32 TotalChangesCount = 0;
	static const int32 DirIndexValue[] = { -1, 1 };

	for (int32 Y = 0; Y < Height; ++Y)
	{
		for (int32 X = 0; X < Width; ++X)
		{
			if (Is3DPointValid(X, Y))
			{
				int32 XLen = 0;
				int32 YLen = 0;

				for (int32 DirIndex = 0; DirIndex < 2; DirIndex++)
				{
					int32 dx = 0;
					int32 dy = 0;

					for (int32 Offset = 1; Offset <= SearchX; Offset++)
					{
						if (Is3DPointValid(X + DirIndexValue[DirIndex] * Offset, Y))
						{
							dx++;
						}
						else
						{
							break;
						}
					}
					for (int32 Offset = 1; Offset <= SearchY; Offset++)
					{
						if (Is3DPointValid(X, Y + DirIndexValue[DirIndex] * Offset))
						{
							dy++;
						}
						else
						{
							break;
						}
					}

					XLen = FMath::Max(XLen, dx);
					YLen = FMath::Max(YLen, dy);
				}

				const bool Test1 = XLen >= Rule1X && YLen >= Rule1Y;
				const bool Test2 = YLen >= Rule1X && XLen >= Rule1Y;

				if (!Test1 && !Test2)
				{
					// Both test failed, remove it
					GetWarpDataPointRef(X,Y) = FVector4f(0.f, 0.f, 0.f, -1.f);
					TotalChangesCount++;
				}
			}
		}
	}

	return TotalChangesCount;
}

