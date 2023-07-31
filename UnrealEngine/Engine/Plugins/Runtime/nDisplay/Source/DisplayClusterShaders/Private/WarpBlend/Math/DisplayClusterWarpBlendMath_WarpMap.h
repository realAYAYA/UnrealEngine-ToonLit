// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Render/DisplayClusterRenderTexture.h"


class FDisplayClusterWarpBlendMath_WarpMap
{
public:
	FDisplayClusterWarpBlendMath_WarpMap(const IDisplayClusterRenderTexture& InWarpMap)
		: Width(InWarpMap.GetWidth())
		, Height(InWarpMap.GetHeight())
		, Data((FVector4f*)InWarpMap.GetData())
	{ }

public:
	FBox GetAABBox() const
	{
		FBox AABBox = FBox(FVector(FLT_MAX, FLT_MAX, FLT_MAX), FVector(-FLT_MAX, -FLT_MAX, -FLT_MAX));

		// Build a bbox from valid points:
		for (int32 Y = 0; Y < Height; ++Y)
		{
			for (int32 X = 0; X < Width; ++X)
			{
				const FVector4f& Pts = GetPoint(X,Y);
				if (Pts.W > 0)
				{
					AABBox.Min.X = FMath::Min(AABBox.Min.X, Pts.X);
					AABBox.Min.Y = FMath::Min(AABBox.Min.Y, Pts.Y);
					AABBox.Min.Z = FMath::Min(AABBox.Min.Z, Pts.Z);

					AABBox.Max.X = FMath::Max(AABBox.Max.X, Pts.X);
					AABBox.Max.Y = FMath::Max(AABBox.Max.Y, Pts.Y);
					AABBox.Max.Z = FMath::Max(AABBox.Max.Z, Pts.Z);
				}
			}
		}

		return AABBox;
	}

	void BuildIndexLOD(int32 DivX, int32 DivY, TArray<int32>& OutIndexLOD)
	{
		check(DivX > 0);
		check(DivY > 0);

		OutIndexLOD.Reserve(DivX * DivY);

		// Generate valid points for texturebox method:
		for (int32 low_y = 0; low_y < DivY; low_y++)
		{
			int32 y = (Height - 1) * (float(low_y) / (DivY - 1));

			for (int32 low_x = 0; low_x < DivX; low_x++)
			{
				int32 x = (Width - 1) * (float(low_x) / (DivX - 1));

				if (IsValidPoint(x, y))
				{
					// Just use direct point
					OutIndexLOD.Add(GetPointIndex(x, y));
				}
				else
				{
					// Search for nearset valid point
					if (FindValidPoint(x, y))
					{
						OutIndexLOD.Add(GetSavedPointIndex());
					}
				}
			}
		}
	}

public:
	int32 GetSavedPointIndex()
	{
		return GetPointIndex(ValidPointX, ValidPointY);
	}

	FORCEINLINE int32 GetPointIndex(int32 InX, int32 InY) const
	{
		return InX + InY * Width;
	}

	FORCEINLINE const FVector4f& GetPoint(int32 InX, int32 InY) const
	{
		return Data[GetPointIndex(InX, InY)];
	}

	FORCEINLINE const FVector4f& GetPoint(int32 PointIndex) const
	{
		return Data[PointIndex];
	}

	bool IsValidPoint(int32 InX, int32 InY)
	{
		return GetPoint(InX, InY).W > 0;
	}

	bool FindValidPoint(int32 InX, int32 InY)
	{
		X0 = InX;
		Y0 = InY;

		for (int32 Range = 1; Range < Width; Range++)
		{
			if (FindValidPointInRange(Range))
			{
				return true;
			}
		}

		return false;
	}

	FVector GetSurfaceViewNormal()
	{
		int32 Ncount = 0;
		double Nxyz[3] = { 0,0,0 };

		for (int32 ItY = 0; ItY < (Height - 2); ++ItY)
		{
			for (int32 ItX = 0; ItX < (Width - 2); ++ItX)
			{
				const FVector4f& Pts0 = GetPoint(ItX, ItY);
				const FVector4f& Pts1 = GetPoint(ItX + 1, ItY);
				const FVector4f& Pts2 = GetPoint(ItX, ItY + 1);

				if (Pts0.W > 0 && Pts1.W > 0 && Pts2.W > 0)
				{
					const FVector N1 = FVector4(Pts1 - Pts0);
					const FVector N2 = FVector4(Pts2 - Pts0);
					const FVector N = FVector::CrossProduct(N2, N1).GetSafeNormal();

					for (int32 AxisIndex = 0; AxisIndex < 3; AxisIndex++)
					{
						Nxyz[AxisIndex] += N[AxisIndex];
					}

					Ncount++;
				}
			}
		}

		double Scale = double(1) / Ncount;
		for (int32 AxisIndex = 0; AxisIndex < 3; AxisIndex++)
		{
			Nxyz[AxisIndex] *= Scale;
		}

		return FVector(Nxyz[0], Nxyz[1], Nxyz[2]).GetSafeNormal();
	}

	FVector GetSurfaceViewPlane()
	{
		const FVector4f& Pts0 = GetValidPoint(0, 0);
		const FVector4f& Pts1 = GetValidPoint(Width - 1, 0);
		const FVector4f& Pts2 = GetValidPoint(0, Height - 1);

		const FVector N1 = FVector4(Pts1 - Pts0);
		const FVector N2 = FVector4(Pts2 - Pts0);
		return FVector::CrossProduct(N2, N1).GetSafeNormal();
	}

private:
	const FVector4f& GetValidPoint(int32 InX, int32 InY)
	{
		if (!IsValidPoint(InX, InY))
		{
			if (FindValidPoint(InX, InY))
			{
				return GetPoint(GetSavedPointIndex());
			}
		}

		return GetPoint(InX, InY);
	}

	bool FindValidPointInRange(int32 Range)
	{
		for (int32 RangeIt = -Range; RangeIt <= Range; RangeIt++)
		{
			// Top or bottom rows
			if (IsValid(X0 + RangeIt, Y0 - Range) || IsValid(X0 + RangeIt, Y0 + Range))
			{
				return true;
			}

			// Left or Right columns
			if (IsValid(X0 - Range, Y0 + RangeIt) || IsValid(X0 + Range, Y0 + RangeIt))
			{
				return true;
			}
		}

		return false;
	}

	bool IsValid(int32 newX, int32 newY)
	{
		if (newX < 0 || newY < 0 || newX >= Width || newY >= Height)
		{
			// Out of texture
			return false;
		}

		if (Data[GetPointIndex(newX, newY)].W > 0)
		{
			// Store valid result
			ValidPointX = newX;
			ValidPointY = newY;

			return true;
		}

		return false;
	}

private:
	const int32 Width;
	const int32 Height;
	const FVector4f* Data;

	// Internal logic
	int32 ValidPointX = 0;
	int32 ValidPointY = 0;

	int32 X0 = 0;
	int32 Y0 = 0;
};
