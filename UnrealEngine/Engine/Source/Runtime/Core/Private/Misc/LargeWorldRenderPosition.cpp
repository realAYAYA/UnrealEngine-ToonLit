// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/LargeWorldRenderPosition.h"

#include "HAL/Platform.h"
#include "Math/Matrix.inl"
#include "Math/TranslationMatrix.h"
#include "Misc/AssertionMacros.h"

#define UE_LWC_RENDER_TILE_SIZE_MIN (262144.0)	// LWC_TODO: Make this smaller?

#define UE_LWC_RENDER_TILE_SIZE 2097152.0
//#define UE_LWC_RENDER_TILE_SIZE UE_OLD_WORLD_MAX (FMath::Max(UE_LWC_RENDER_TILE_SIZE_MIN,  HALF_WORLD_MAX / 16777216.0))	// LWC_TODO: Tweak render tile divisor to maximise range.
//static_assert(UE_LWC_RENDER_TILE_SIZE <= 1048576.0, "Current WORLD_MAX is too large and is likely to adversely affect world space coordinate precision within shaders!");	// LWC_TODO: Set this to something reasonable.

//#define UE_LWC_RENDER_TILE_SIZE (262144.0)
//#define UE_LWC_RENDER_TILE_SIZE (1048576.0)
//#define UE_LWC_RENDER_TILE_SIZE (2097152.0)

// This is the max size we allow for LWC offsets relative to the tile
// Value chosen to ensure sufficient precision when stored in single precision float
// Normally offsets should be within +/-TileSizeDivideBy2, but we often rebase multiple quantities off a single tile origin
#define UE_LWC_RENDER_MAX_OFFSET (2097152.0*0.5)

double FLargeWorldRenderScalar::GetTileSize()
{
	return UE_LWC_RENDER_TILE_SIZE;
}

FVector3f FLargeWorldRenderScalar::GetTileFor(FVector InPosition)
{
	if constexpr (UE_LWC_RENDER_TILE_SIZE == 0)
	{
		return FVector3f::ZeroVector;
	}
	FVector3f LWCTile = FVector3f(InPosition / UE_LWC_RENDER_TILE_SIZE + 0.5);

	// normalize the tile
	LWCTile.X = FMath::FloorToFloat(LWCTile.X);
	LWCTile.Y = FMath::FloorToFloat(LWCTile.Y);
	LWCTile.Z = FMath::FloorToFloat(LWCTile.Z);

	return LWCTile;
}

FMatrix CheckMatrixInTileOffsetRange(const FMatrix& Matrix)
{
	const double OriginMax = UE_LWC_RENDER_MAX_OFFSET;

	const FVector Origin = Matrix.GetOrigin();
	const double OriginX = FMath::Abs(Origin.X);
	const double OriginY = FMath::Abs(Origin.Y);
	const double OriginZ = FMath::Abs(Origin.Z);
	if (OriginX > OriginMax || OriginY > OriginMax || OriginZ > OriginMax)
	{
		ensure(false);
	}
	return Matrix;
}

FMatrix44f FLargeWorldRenderScalar::SafeCastMatrix(const FMatrix& Matrix)
{
	return FMatrix44f(CheckMatrixInTileOffsetRange(Matrix));
}

FMatrix44f FLargeWorldRenderScalar::MakeToRelativeWorldMatrix(const FVector Origin, const FMatrix& ToWorld)
{
	return FMatrix44f(MakeToRelativeWorldMatrixDouble(Origin, ToWorld));
}

FMatrix FLargeWorldRenderScalar::MakeToRelativeWorldMatrixDouble(const FVector Origin, const FMatrix& ToWorld)
{
	return CheckMatrixInTileOffsetRange(ToWorld * FTranslationMatrix(-Origin));
}

FMatrix44f FLargeWorldRenderScalar::MakeFromRelativeWorldMatrix(const FVector Origin, const FMatrix& FromWorld)
{
	return FMatrix44f(MakeFromRelativeWorldMatrixDouble(Origin, FromWorld));
}

FMatrix FLargeWorldRenderScalar::MakeFromRelativeWorldMatrixDouble(const FVector Origin, const FMatrix& FromWorld)
{
	return CheckMatrixInTileOffsetRange(FTranslationMatrix(Origin) * FromWorld);
}

FMatrix44f FLargeWorldRenderScalar::MakeClampedToRelativeWorldMatrix(const FVector Origin, const FMatrix& ToWorld)
{
	return FMatrix44f(MakeClampedToRelativeWorldMatrixDouble(Origin, ToWorld));
}

FMatrix FLargeWorldRenderScalar::MakeClampedToRelativeWorldMatrixDouble(const FVector Origin, const FMatrix& ToWorld)
{
	const double OriginMax = UE_LWC_RENDER_MAX_OFFSET;

	// Clamp the relative matrix, avoid allowing the relative translation to get too far away from the tile origin
	const FVector RelativeOrigin = ToWorld.GetOrigin() - Origin;
	FVector ClampedRelativeOrigin = RelativeOrigin;
	ClampedRelativeOrigin.X = FMath::Clamp(ClampedRelativeOrigin.X, -OriginMax, OriginMax);
	ClampedRelativeOrigin.Y = FMath::Clamp(ClampedRelativeOrigin.Y, -OriginMax, OriginMax);
	ClampedRelativeOrigin.Z = FMath::Clamp(ClampedRelativeOrigin.Z, -OriginMax, OriginMax);

	FMatrix ClampedToRelativeWorld(ToWorld);
	ClampedToRelativeWorld.SetOrigin(ClampedRelativeOrigin);
	return ClampedToRelativeWorld;
}

void FLargeWorldRenderScalar::Validate(double InAbsolute)
{
	const double Tolerance = 0.01; // TODO LWC - How precise do we need to be?
	const double CheckAbsolute = GetAbsolute();
	const double Delta = FMath::Abs(CheckAbsolute - InAbsolute);

	ensureMsgf(Delta < Tolerance, TEXT("Bad FLargeWorldRenderScalar (%g) vs (%g)"),
		InAbsolute, CheckAbsolute);
}
