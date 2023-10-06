// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components.h"

// Lerpable version of FStaticMeshBuildVertex
struct FLerpVert
{
	FVector3f		Position;

	FVector3f		TangentX;
	FVector3f		TangentY;
	FVector3f		TangentZ;

	FLinearColor	Color;
	FVector2f		UVs[MAX_STATIC_TEXCOORDS];

	FLerpVert() {}
	FLerpVert( FStaticMeshBuildVertex In )
		: Position( In.Position )
		, TangentX( In.TangentX )
		, TangentY( In.TangentY )
		, TangentZ( In.TangentZ )
	{
		for( uint32 i = 0; i < MAX_STATIC_TEXCOORDS; i++ )
			UVs[i] = In.UVs[i];

		Color = In.Color.ReinterpretAsLinear();
	}

	operator FStaticMeshBuildVertex() const
	{
		FStaticMeshBuildVertex Vert;
		Vert.Position = Position;
		Vert.TangentX = TangentX;
		Vert.TangentY = TangentY;
		Vert.TangentZ = TangentZ;
		Vert.Color    = Color.ToFColor( false );
		
		for( uint32 i = 0; i < MAX_STATIC_TEXCOORDS; i++ )
			Vert.UVs[i] = UVs[i];

		return Vert;
	}

	FLerpVert& operator+=( const FLerpVert& Other )
	{
		Position += Other.Position;
		TangentX += Other.TangentX;
		TangentY += Other.TangentY;
		TangentZ += Other.TangentZ;
		Color    += Other.Color;

		for( uint32 i = 0; i < MAX_STATIC_TEXCOORDS; i++ )
			UVs[i] += Other.UVs[i];

		return *this;
	}

	FLerpVert operator*( const float a ) const
	{
		FLerpVert Vert;
		Vert.Position = Position * a;
		Vert.TangentX = TangentX * a;
		Vert.TangentY = TangentY * a;
		Vert.TangentZ = TangentZ * a;
		Vert.Color    = Color * a;
		
		for( uint32 i = 0; i < MAX_STATIC_TEXCOORDS; i++ )
			Vert.UVs[i] = UVs[i] * a;

		return Vert;
	}
};