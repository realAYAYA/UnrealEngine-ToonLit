// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components.h"
#include "DisplacementMap.h"
#include "Affine.h"
#include "LerpVert.h"
#include "AdaptiveTessellator.h"
#include "Math/Bounds.h"
#include "Engine/EngineTypes.h"
#include "Engine/Texture2D.h"

namespace Nanite
{

static FVector3f GetDisplacement(
	const FVector3f& Barycentrics,
	const FLerpVert& Vert0,
	const FLerpVert& Vert1,
	const FLerpVert& Vert2,
	int32 MaterialIndex,
	int32 UVIndex,
	TArrayView< FDisplacementMap > DisplacementMaps )
{
	FVector2f UV;
	UV  = Vert0.UVs[ UVIndex ] * Barycentrics.X;
	UV += Vert1.UVs[ UVIndex ] * Barycentrics.Y;
	UV += Vert2.UVs[ UVIndex ] * Barycentrics.Z;

	FVector3f Normal;
	Normal  = Vert0.TangentZ * Barycentrics.X;
	Normal += Vert1.TangentZ * Barycentrics.Y;
	Normal += Vert2.TangentZ * Barycentrics.Z;
	Normal.Normalize();

	float Displacement = 0.0f;
	if( DisplacementMaps.IsValidIndex( MaterialIndex ) )
		Displacement = DisplacementMaps[ MaterialIndex ].Sample( UV );

	return Normal * Displacement;
}

static FVector2f GetErrorBounds(
	const FVector3f Barycentrics[3],
	const FLerpVert& Vert0,
	const FLerpVert& Vert1,
	const FLerpVert& Vert2,
	const FVector3f& Displacement0,
	const FVector3f& Displacement1,
	const FVector3f& Displacement2,
	int32 MaterialIndex,
	int32 UVIndex,
	TArrayView< FDisplacementMap > DisplacementMaps )
{
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
	Normal  = TAffine< FVector3f, 2 >( Vert0.TangentZ ) * Barycentric0;
	Normal += TAffine< FVector3f, 2 >( Vert1.TangentZ ) * Barycentric1;
	Normal += TAffine< FVector3f, 2 >( Vert2.TangentZ ) * Barycentric2;
	Normal = Normalize( Normal );

	FVector2f MinUV = {  MAX_flt,  MAX_flt };
	FVector2f MaxUV = { -MAX_flt, -MAX_flt };
	for( int k = 0; k < 3; k++ )
	{
		FVector2f UV;
		UV  = Vert0.UVs[ UVIndex ] * Barycentrics[k].X;
		UV += Vert1.UVs[ UVIndex ] * Barycentrics[k].Y;
		UV += Vert2.UVs[ UVIndex ] * Barycentrics[k].Z;

		MinUV = FVector2f::Min( MinUV, UV );
		MaxUV = FVector2f::Max( MaxUV, UV );
	}

	FVector2f DisplacementBounds( 0.0f, 0.0f );
	if( DisplacementMaps.IsValidIndex( MaterialIndex ) )
		DisplacementBounds = DisplacementMaps[ MaterialIndex ].Sample( MinUV, MaxUV );
	
	TAffine< float, 2 > Displacement( DisplacementBounds.X, DisplacementBounds.Y );
	TAffine< float, 2 > Error = ( Normal * Displacement - LerpedDisplacement ).SizeSquared();

	return FVector2f( Error.GetMin(), Error.GetMax() );
}

void TessellateAndDisplace(
	FMeshBuildVertexData& Verts,
	TArray< uint32 >& Indexes,
	TArray< int32 >& MaterialIndexes,
	const FBounds3f& MeshBounds,
	const FMeshNaniteSettings& Settings )
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Nanite::Build::TessellateAndDisplace);

	float SurfaceArea = 0.0f;
	for( int32 TriIndex = 0; TriIndex < MaterialIndexes.Num(); TriIndex++ )
	{
		auto& Vert0Position = Verts.Position[ Indexes[ TriIndex * 3 + 0 ] ];
		auto& Vert1Position = Verts.Position[ Indexes[ TriIndex * 3 + 1 ] ];
		auto& Vert2Position = Verts.Position[ Indexes[ TriIndex * 3 + 2 ] ];

		FVector3f Edge01 = Vert1Position - Vert0Position;
		FVector3f Edge12 = Vert2Position - Vert1Position;
		FVector3f Edge20 = Vert0Position - Vert2Position;

		SurfaceArea += 0.5f * ( Edge01 ^ Edge20 ).Size();
	}

	float TargetError = Settings.TrimRelativeError * 0.01f * FMath::Sqrt( FMath::Min( 2.0f * SurfaceArea, MeshBounds.GetSurfaceArea() ) );

	// Overtessellate by 50% and simplify down
	TargetError *= 1.5f;

	TArray< FDisplacementMap > DisplacementMaps;
	for( auto& DisplacementMap : Settings.DisplacementMaps )
	{
		if ( DisplacementMap.Texture )
		{
			if ( IsValid( DisplacementMap.Texture ) && DisplacementMap.Texture->Source.IsValid() )
			{
				FImage FirstMipImage;
				if ( DisplacementMap.Texture->Source.GetMipImage( FirstMipImage, 0 ) )
				{
					DisplacementMaps.Add( Nanite::FDisplacementMap(
						MoveTemp( FirstMipImage ),
						DisplacementMap.Magnitude,
						DisplacementMap.Center,
						DisplacementMap.Texture->AddressX,
						DisplacementMap.Texture->AddressY ) );
				}
				else
				{
					// Virtualization can fail to fetch the bulk data.
					checkNoEntry();
				}
			}
			else
			{
				// If the raw pointer is not null, but for some reason it is not valid better to crash then continuing with some false data
				checkNoEntry();
			}
		}
		else
		{
			DisplacementMaps.AddDefaulted();
		}
	}

	TArray< FLerpVert >	LerpVerts;
	LerpVerts.AddUninitialized( Verts.Position.Num() );
	for( int i = 0; i < Verts.Position.Num(); i++ )
		LerpVerts[i] = MakeStaticMeshVertex(Verts, i);

	FAdaptiveTessellator Tessellator( LerpVerts, Indexes, MaterialIndexes, TargetError, TargetError, true,
		[&](const FVector3f& Barycentrics,
			const FLerpVert& Vert0,
			const FLerpVert& Vert1,
			const FLerpVert& Vert2,
			int32 MaterialIndex )
		{
			return GetDisplacement(
				Barycentrics,
				Vert0,
				Vert1,
				Vert2,
				MaterialIndex,
				Settings.DisplacementUVChannel,
				DisplacementMaps );
		},
		[&](const FVector3f Barycentrics[3],
			const FLerpVert& Vert0,
			const FLerpVert& Vert1,
			const FLerpVert& Vert2,
			const FVector3f& Displacement0,
			const FVector3f& Displacement1,
			const FVector3f& Displacement2,
			int32 MaterialIndex )
		{
			return GetErrorBounds(
				Barycentrics,
				Vert0,
				Vert1,
				Vert2,
				Displacement0,
				Displacement1,
				Displacement2,
				MaterialIndex,
				Settings.DisplacementUVChannel,
				DisplacementMaps );
		} );

	const bool bHasVertexColor = Verts.Color.Num() > 0;
	
	Verts.Position.SetNumUninitialized(LerpVerts.Num());
	Verts.TangentX.SetNumUninitialized(LerpVerts.Num());
	Verts.TangentY.SetNumUninitialized(LerpVerts.Num());
	Verts.TangentZ.SetNumUninitialized(LerpVerts.Num());

	for (int32 UVCoord = 0; UVCoord < Verts.UVs.Num(); ++UVCoord)
	{
		Verts.UVs[UVCoord].SetNumUninitialized(LerpVerts.Num());
	}

	Verts.Color.SetNumUninitialized(bHasVertexColor ? LerpVerts.Num() : 0);

	for (int32 LerpIndex = 0; LerpIndex < LerpVerts.Num(); ++LerpIndex)
	{
		const FLerpVert& LerpVert = LerpVerts[LerpIndex];
		Verts.Position[LerpIndex] = LerpVert.Position;
		Verts.TangentX[LerpIndex] = LerpVert.TangentX;
		Verts.TangentY[LerpIndex] = LerpVert.TangentY;
		Verts.TangentZ[LerpIndex] = LerpVert.TangentZ;

		for (int32 UVCoord = 0; UVCoord < Verts.UVs.Num(); ++UVCoord)
		{
			Verts.UVs[UVCoord][LerpIndex] = LerpVert.UVs[UVCoord];
		}

		if (bHasVertexColor)
		{
			Verts.Color[LerpIndex] = LerpVert.Color.ToFColor(false);
		}
	}
}

} // namespace Nanite
