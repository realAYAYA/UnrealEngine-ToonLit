// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// Example vertex type for TMeshSimplifier
template< uint32 NumTexCoords >
class TVertSimp
{
	typedef TVertSimp< NumTexCoords > VertType;
public:
	FVector3f			Position;
	FVector3f			Normal;
	FVector3f			Tangents[2];
	FLinearColor		Color;
	FVector2f			TexCoords[ NumTexCoords ];

	FVector3f&		GetPos()					{ return Position; }
	const FVector3f&	GetPos() const				{ return Position; }
	float*			GetAttributes()				{ return (float*)&Normal; }
	const float*	GetAttributes() const		{ return (const float*)&Normal; }

	void		Correct()
	{
		Normal.Normalize();
		Tangents[0] -= ( Tangents[0] | Normal ) * Normal;
		Tangents[0].Normalize();
		Tangents[1] -= ( Tangents[1] | Normal ) * Normal;
		Tangents[1] -= ( Tangents[1] | Tangents[0] ) * Tangents[0];
		Tangents[1].Normalize();
		Color = Color.GetClamped();
	}

	bool		Equals(	const VertType& a ) const
	{
		if( !PointsEqual( (FVector)Position, (FVector)a.Position ) ||
			!NormalsEqual( (FVector)Tangents[0], (FVector)a.Tangents[0] ) ||
			!NormalsEqual( (FVector)Tangents[1], (FVector)a.Tangents[1] ) ||
			!NormalsEqual( (FVector)Normal, (FVector)a.Normal ) ||
			!Color.Equals( a.Color ) )
		{
			return false;
		}

		// UVs
		for( int32 UVIndex = 0; UVIndex < NumTexCoords; UVIndex++ )
		{
			if( !UVsEqual( TexCoords[ UVIndex ], a.TexCoords[ UVIndex ] ) )
			{
				return false;
			}
		}

		return true;
	}

	bool		operator==(	const VertType& a ) const
	{
		if( Position		!= a.Position ||
			Normal			!= a.Normal ||
			Tangents[0]		!= a.Tangents[0] ||
			Tangents[1]		!= a.Tangents[1] ||
			Color			!= a.Color )
		{
			return false;
		}

		for( uint32 i = 0; i < NumTexCoords; i++ )
		{
			if( TexCoords[i] != a.TexCoords[i] )
			{
				return false;
			}
		}
		return true;
	}

	VertType	operator+( const VertType& a ) const
	{
		VertType v;
		v.Position		= Position + a.Position;
		v.Normal		= Normal + a.Normal;
		v.Tangents[0]	= Tangents[0] + a.Tangents[0];
		v.Tangents[1]	= Tangents[1] + a.Tangents[1];
		v.Color			= Color + a.Color;

		for( uint32 i = 0; i < NumTexCoords; i++ )
		{
			v.TexCoords[i] = TexCoords[i] + a.TexCoords[i];
		}
		return v;
	}

	VertType	operator-( const VertType& a ) const
	{
		VertType v;
		v.Position		= Position - a.Position;
		v.Normal		= Normal - a.Normal;
		v.Tangents[0]	= Tangents[0] - a.Tangents[0];
		v.Tangents[1]	= Tangents[1] - a.Tangents[1];
		v.Color			= Color - a.Color;
		
		for( uint32 i = 0; i < NumTexCoords; i++ )
		{
			v.TexCoords[i] = TexCoords[i] - a.TexCoords[i];
		}
		return v;
	}

	VertType	operator*( const float a ) const
	{
		VertType v;
		v.Position		= Position * a;
		v.Normal		= Normal * a;
		v.Tangents[0]	= Tangents[0] * a;
		v.Tangents[1]	= Tangents[1] * a;
		v.Color			= Color * a;
		
		for( uint32 i = 0; i < NumTexCoords; i++ )
		{
			v.TexCoords[i] = TexCoords[i] * a;
		}
		return v;
	}

	VertType	operator/( const float a ) const
	{
		float ia = 1.0f / a;
		return (*this) * ia;
	}
};