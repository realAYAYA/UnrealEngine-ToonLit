// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Math/IntPoint.h"
#include "Math/IntRect.h"
#include "Math/IntVector.h"
#include "Math/MathFwd.h"

namespace UE::NNEDenoiser::Private
{
	
	struct FTile
	{
		FIntPoint Position;
		FIntRect OutputOffsets;
	};

	struct FTiling
	{
		FIntPoint TileSize;
		FIntPoint Count;
		TArray<FTile> Tiles;
	};

	inline FTiling CreateTiling(FIntPoint TargetTileSize, FIntPoint MinimumOverlap, FIntPoint Size)
	{
		auto GetNumTiles = [] (int32 TileSize, int32 Overlap, int32 Size)
		{
			return FMath::Max(FMath::CeilToInt32((Size - TileSize) / (float)(TileSize - Overlap)), 0) + 1;
		};

		auto GetOffsets = [] (int32 Count, int32 TileSize, int32 Overlap, int32 Size)
		{
			TArray<int32> Result;
			for (int32 I = 0; I < Count; I++)
			{
				Result.Add(I < Count - 1 ? I * (TileSize - Overlap) : Size - TileSize);
			}

			return Result;
		};

		FTiling Result{};
		Result.TileSize = {
			FMath::Min(TargetTileSize.X, Size.X),
			FMath::Min(TargetTileSize.Y, Size.Y)
		};

		Result.Count = {
			GetNumTiles(Result.TileSize.X, MinimumOverlap.X, Size.X),
			GetNumTiles(Result.TileSize.Y, MinimumOverlap.Y, Size.Y)
		};

		const FIntPoint TotalOverlap = Result.Count * Result.TileSize - Size;
		const FIntPoint Overlap = {
			Result.Count.X == 1 ? 0 : TotalOverlap.X / (Result.Count.X - 1),
			Result.Count.Y == 1 ? 0 : TotalOverlap.Y / (Result.Count.Y - 1)
		};

		const FIntPoint HalfOverlap = {
			FMath::FloorToInt32(Overlap.X / 2.0f),
			FMath::FloorToInt32(Overlap.Y / 2.0f)
		};

		const TArray<int32> OffsetsX = GetOffsets(Result.Count.X, Result.TileSize.X, Overlap.X, Size.X);
		const TArray<int32> OffsetsY = GetOffsets(Result.Count.Y, Result.TileSize.Y, Overlap.Y, Size.Y);

		for (int32 Ty = 0; Ty < Result.Count.Y; Ty++)
		{
			const int32 Y0 = OffsetsY[Ty];
			const int32 Y1 = Y0 + Result.TileSize.Y;

			const int32 OutY0 = Ty > 0 ? HalfOverlap.Y : 0;
			const int32 OutY1 = Ty < Result.Count.Y - 1 ? -HalfOverlap.Y : 0;

			for (int32 Tx = 0; Tx < Result.Count.X; Tx++)
			{
				const int32 X0 = OffsetsX[Tx];
				const int32 X1 = X0 + Result.TileSize.X;

				const int32 OutX0 = Tx > 0 ? HalfOverlap.X : 0;
				const int32 OutX1 = Tx < Result.Count.X - 1 ? -HalfOverlap.X : 0;

				const FIntPoint InputPosition = {X0, Y0};
				const FIntRect OutputOffsets = {OutX0, OutY0, OutX1, OutY1};

				Result.Tiles.Add(FTile{InputPosition, OutputOffsets});
			}
		}

		return Result;
	}

}