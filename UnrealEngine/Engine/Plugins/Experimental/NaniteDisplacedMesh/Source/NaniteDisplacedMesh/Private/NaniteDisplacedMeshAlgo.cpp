// Copyright Epic Games, Inc. All Rights Reserved.

#include "NaniteDisplacedMeshAlgo.h"

#if WITH_EDITOR

#include "NaniteDisplacedMesh.h"
#include "DisplacementMap.h"
#include "Affine.h"
#include "LerpVert.h"
#include "AdaptiveTessellator.h"
#include "Math/Bounds.h"

static FVector3f UserGetDisplacement(
	const FVector3f& Barycentrics,
	const FLerpVert& Vert0,
	const FLerpVert& Vert1,
	const FLerpVert& Vert2,
	TArrayView< Nanite::FDisplacementMap > DisplacementMaps )
{
	int32 DisplacementIndex = FMath::FloorToInt( Vert0.UVs[1].X );
	if( !DisplacementMaps.IsValidIndex( DisplacementIndex ) )
		return FVector3f( 0.0f, 0.0f, 0.0f );

	FVector2f UV;
	UV  = Vert0.UVs[0] * Barycentrics.X;
	UV += Vert1.UVs[0] * Barycentrics.Y;
	UV += Vert2.UVs[0] * Barycentrics.Z;

	FVector3f Normal;
	Normal  = Vert0.TangentX * Barycentrics.X;
	Normal += Vert1.TangentX * Barycentrics.Y;
	Normal += Vert2.TangentX * Barycentrics.Z;
	Normal.Normalize();

	float Displacement = DisplacementMaps[ DisplacementIndex ].Sample( UV );

	return Normal * Displacement;
}

static FVector2f UserGetErrorBounds(
	const FVector3f Barycentrics[3],
	const FLerpVert& Vert0,
	const FLerpVert& Vert1,
	const FLerpVert& Vert2,
	const FVector3f& Displacement0,
	const FVector3f& Displacement1,
	const FVector3f& Displacement2,
	TArrayView< Nanite::FDisplacementMap > DisplacementMaps )
{
	// Assume index is constant across triangle
	int32 DisplacementIndex = FMath::FloorToInt( Vert0.UVs[1].X );
	if( !DisplacementMaps.IsValidIndex( DisplacementIndex ) )
		return FVector2f( 0.0f, 0.0f );

#if 1
	float MinBarycentric0 = FMath::Min3( Barycentrics[0].X, Barycentrics[1].X, Barycentrics[2].X );
	float MaxBarycentric0 = FMath::Max3( Barycentrics[0].X, Barycentrics[1].X, Barycentrics[2].X );

	float MinBarycentric1 = FMath::Min3( Barycentrics[0].Y, Barycentrics[1].Y, Barycentrics[2].Y );
	float MaxBarycentric1 = FMath::Max3( Barycentrics[0].Y, Barycentrics[1].Y, Barycentrics[2].Y );

	TAffine< float, 2 > Barycentric0( MinBarycentric0, MaxBarycentric0, 0 );
	TAffine< float, 2 > Barycentric1( MinBarycentric1, MaxBarycentric1, 1 );
	TAffine< float, 2 > Barycentric2 = TAffine< float, 2 >( 1.0f ) - Barycentric0 - Barycentric1;

	TAffine< FVector3f, 2 > LerpedDisplacement;
	LerpedDisplacement  = TAffine< FVector3f, 2 >( Displacement0 ) * Barycentric0;
	LerpedDisplacement += TAffine< FVector3f, 2 >( Displacement1 ) * Barycentric1;
	LerpedDisplacement += TAffine< FVector3f, 2 >( Displacement2 ) * Barycentric2;

	TAffine< FVector3f, 2 > Normal;
	Normal  = TAffine< FVector3f, 2 >( Vert0.TangentX ) * Barycentric0;
	Normal += TAffine< FVector3f, 2 >( Vert1.TangentX ) * Barycentric1;
	Normal += TAffine< FVector3f, 2 >( Vert2.TangentX ) * Barycentric2;
	Normal = Normalize( Normal );

	FVector2f MinUV = {  MAX_flt,  MAX_flt };
	FVector2f MaxUV = { -MAX_flt, -MAX_flt };
	for( int k = 0; k < 3; k++ )
	{
		FVector2f UV;
		UV  = Vert0.UVs[0] * Barycentrics[k].X;
		UV += Vert1.UVs[0] * Barycentrics[k].Y;
		UV += Vert2.UVs[0] * Barycentrics[k].Z;

		MinUV = FVector2f::Min( MinUV, UV );
		MaxUV = FVector2f::Max( MaxUV, UV );
	}

	FVector2f DisplacementBounds = DisplacementMaps[ DisplacementIndex ].Sample( MinUV, MaxUV );
	
	TAffine< float, 2 > Displacement( DisplacementBounds.X, DisplacementBounds.Y );
	TAffine< float, 2 > Error = ( Normal * Displacement - LerpedDisplacement ).SizeSquared();

	return FVector2f( Error.GetMin(), Error.GetMax() );
#else
	float ScalarDisplacement0 = Displacement0.Length();
	float ScalarDisplacement1 = Displacement1.Length();
	float ScalarDisplacement2 = Displacement2.Length();	
	float LerpedDisplacement[3];
	
	FVector2f MinUV = {  MAX_flt,  MAX_flt };
	FVector2f MaxUV = { -MAX_flt, -MAX_flt };
	for( int k = 0; k < 3; k++ )
	{
		LerpedDisplacement[k]  = ScalarDisplacement0 * Barycentrics[k].X;
		LerpedDisplacement[k] += ScalarDisplacement1 * Barycentrics[k].Y;
		LerpedDisplacement[k] += ScalarDisplacement2 * Barycentrics[k].Z;

		FVector2f UV;
		UV  = Vert0.UVs[0] * Barycentrics[k].X;
		UV += Vert1.UVs[0] * Barycentrics[k].Y;
		UV += Vert2.UVs[0] * Barycentrics[k].Z;

		MinUV = FVector2f::Min( MinUV, UV );
		MaxUV = FVector2f::Max( MaxUV, UV );
	}
	
	FVector2f LerpedBounds;
	LerpedBounds.X = FMath::Min3( LerpedDisplacement[0], LerpedDisplacement[1], LerpedDisplacement[2] );
	LerpedBounds.Y = FMath::Max3( LerpedDisplacement[0], LerpedDisplacement[1], LerpedDisplacement[2] );

	FVector2f DisplacementBounds = DisplacementMaps[ DisplacementIndex ].Sample( MinUV, MaxUV );

	FVector2f Delta( DisplacementBounds.X - LerpedBounds.Y, DisplacementBounds.Y - LerpedBounds.X );
	FVector2f DeltaSqr = Delta * Delta;

	FVector2f ErrorBounds;
	ErrorBounds.X = DeltaSqr.GetMin();
	ErrorBounds.Y = DeltaSqr.GetMax();
	if( Delta.X * Delta.Y < 0.0f )
		ErrorBounds.X = 0.0f;
#endif
}

bool DisplaceNaniteMesh(
	const FNaniteDisplacedMeshParams& Parameters,
	const uint32 NumTextureCoord,
	TArray< FStaticMeshBuildVertex >& Verts,
	TArray< uint32 >& Indexes,
	TArray< int32 >& MaterialIndexes
)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(DisplaceNaniteMesh);

	uint32 Time0 = FPlatformTime::Cycles();

	// TODO: Make the mesh prepare and displacement logic extensible, and not hardcoded within this plugin

	// START - MESH PREPARE

	TArray<uint32> VertSamples;
	VertSamples.SetNumZeroed(Verts.Num());

	ParallelFor(TEXT("Nanite.Displace.Guide"), Verts.Num(), 1024,
	[&](int32 VertIndex)
	{
		FStaticMeshBuildVertex& TargetVert = Verts[VertIndex];

		TargetVert.TangentX = FVector3f::ZeroVector;

		for (int32 GuideVertIndex = 0; GuideVertIndex < Verts.Num(); ++GuideVertIndex)
		{
			const FStaticMeshBuildVertex& GuideVert = Verts[GuideVertIndex];

			if (GuideVert.UVs[1].Y >= 0.0f)
			{
				continue;
			}

			FVector3f GuideVertPos = GuideVert.Position;

			// Matches the geoscript prototype (TODO: Remove)
			const bool bApplyTolerance = true;
			if (bApplyTolerance)
			{
			    float Tolerance = 0.01f;
			    GuideVertPos /= Tolerance;
			    GuideVertPos.X = float(FMath::CeilToInt(GuideVertPos.X)) * Tolerance;
			    GuideVertPos.Y = float(FMath::CeilToInt(GuideVertPos.Y)) * Tolerance;
			    GuideVertPos.Z = float(FMath::CeilToInt(GuideVertPos.Z)) * Tolerance;
			}

			if (FVector3f::Distance(TargetVert.Position, GuideVertPos) < 0.1f)
			{
				++VertSamples[VertIndex];
				TargetVert.TangentX += GuideVert.TangentZ;
			}
		}

		if (VertSamples[VertIndex] > 0)
		{
			TargetVert.TangentX /= VertSamples[VertIndex];
			TargetVert.TangentX.Normalize();
		}
	});
	// END - MESH PREPARE

	FBounds3f Bounds;
	for( auto& Vert : Verts )
		Bounds += Vert.Position;

	float SurfaceArea = 0.0f;
	for( int32 TriIndex = 0; TriIndex < MaterialIndexes.Num(); TriIndex++ )
	{
		auto& Vert0 = Verts[ Indexes[ TriIndex * 3 + 0 ] ];
		auto& Vert1 = Verts[ Indexes[ TriIndex * 3 + 1 ] ];
		auto& Vert2 = Verts[ Indexes[ TriIndex * 3 + 2 ] ];

		FVector3f Edge01 = Vert1.Position - Vert0.Position;
		FVector3f Edge12 = Vert2.Position - Vert1.Position;
		FVector3f Edge20 = Vert0.Position - Vert2.Position;

		SurfaceArea += 0.5f * ( Edge01 ^ Edge20 ).Size();
	}

	float TargetError = Parameters.RelativeError * 0.01f * FMath::Sqrt( FMath::Min( 2.0f * SurfaceArea, Bounds.GetSurfaceArea() ) );

	// Overtessellate by 50% and simplify down
	TargetError *= 1.5f;

	TArray< Nanite::FDisplacementMap > DisplacementMaps;
	for( auto& DisplacementMap : Parameters.DisplacementMaps )
	{
		if( IsValid( DisplacementMap.Texture ) )
			DisplacementMaps.Add( Nanite::FDisplacementMap( DisplacementMap.Texture->Source, DisplacementMap.Magnitude, DisplacementMap.Center ) );
		else
			DisplacementMaps.AddDefaulted();
	}

	TArray< FLerpVert >	LerpVerts;
	LerpVerts.AddUninitialized( Verts.Num() );
	for( int i = 0; i < Verts.Num(); i++ )
		LerpVerts[i] = Verts[i];

	Nanite::FAdaptiveTessellator Tessellator( LerpVerts, Indexes, MaterialIndexes, TargetError, TargetError, true,
		[&](const FVector3f& Barycentrics,
			const FLerpVert& Vert0,
			const FLerpVert& Vert1,
			const FLerpVert& Vert2 )
		{
			return UserGetDisplacement(
				Barycentrics,
				Vert0,
				Vert1,
				Vert2,
				DisplacementMaps );
		},
		[&](const FVector3f Barycentrics[3],
			const FLerpVert& Vert0,
			const FLerpVert& Vert1,
			const FLerpVert& Vert2,
			const FVector3f& Displacement0,
			const FVector3f& Displacement1,
			const FVector3f& Displacement2 )
		{
			return UserGetErrorBounds(
				Barycentrics,
				Vert0,
				Vert1,
				Vert2,
				Displacement0,
				Displacement1,
				Displacement2,
				DisplacementMaps );
		} );

	Verts.SetNumUninitialized( LerpVerts.Num() );
	for( int i = 0; i < Verts.Num(); i++ )
		Verts[i] = LerpVerts[i];

	uint32 Time1 = FPlatformTime::Cycles();
	UE_LOG( LogStaticMesh, Log, TEXT("Adaptive tessellate [%.2fs], tris: %i"), FPlatformTime::ToMilliseconds( Time1 - Time0 ) / 1000.0f, Indexes.Num() / 3 );

	return true;
}

#endif
