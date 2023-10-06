// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "WarpBlend/DisplayClusterWarpEnums.h"


namespace mpcdi
{
	struct GeometryWarpFile;
	struct PFM;
}
class FDisplayClusterWarpBlendLoader_WarpMap;

class FLoadedWarpMapData
{
public:
	~FLoadedWarpMapData();

public:
	int32 GetWidth() const
	{
		return Width;
	}

	int32 GetHeight() const
	{
		return Height;
	}

	const FVector4f* GetWarpData() const
	{
		return WarpData;
	}

protected:
	friend FDisplayClusterWarpBlendLoader_WarpMap;

	bool Initialize(int32 InWidth, int32 InHeight);

	void LoadGeometry(EDisplayClusterWarpProfileType ProfileType, const TArray<FVector>& InPoints, float WorldScale, bool bIsUnrealGameSpace);
	bool Is3DPointValid(int32 X, int32 Y) const;
	void ClearNoise(const FIntPoint& SearchXYDepth, const FIntPoint& AllowedXYDepthRules);
	int32 RemoveDetachedPoints(const FIntPoint& SearchLen, const FIntPoint& RemoveRule);

	FORCEINLINE const FVector4f& GetWarpDataPointConstRef(int32 X, int32 Y) const
	{
		check(WarpData);
		check(X >= 0 && X < Width);
		check(Y >= 0 && Y < Height);

		return WarpData[X + Y * Width];
	}

	FORCEINLINE FVector4f& GetWarpDataPointRef(int32 X, int32 Y)
	{
		check(WarpData);
		check(X >= 0 && X < Width);
		check(Y >= 0 && Y < Height);

		return WarpData[X + Y * Width];
	}

protected:
	int32 Width = 0;
	int32 Height = 0;
	FVector4f* WarpData = nullptr;
};

class FDisplayClusterWarpBlendLoader_WarpMap
{
public:
	static bool Load(FLoadedWarpMapData& OutWarpMapData, EDisplayClusterWarpProfileType InProfileType, mpcdi::GeometryWarpFile* SourceWarpMap);
	static bool Load(FLoadedWarpMapData& OutWarpMapData, EDisplayClusterWarpProfileType InProfileType, const TArray<FVector>& InPoints, int32 WarpX, int32 WarpY, float WorldScale, bool bIsUnrealGameSpace);
	static bool Load(FLoadedWarpMapData& OutWarpMapData, EDisplayClusterWarpProfileType InProfileType, mpcdi::PFM* SourcePFM, float PFMScale, bool bIsUnrealGameSpace);
};

