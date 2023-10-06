// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace AbsHelper
{
	template< typename T >
	T Abs( T A )
	{
		return FMath::Abs( A );
	}

	template< typename T >
	UE::Math::TVector2<T> Abs( const UE::Math::TVector2<T>& A )
	{
		return A.GetAbs();
	}

	template< typename T >
	UE::Math::TVector<T> Abs( const UE::Math::TVector<T>& A )
	{
		return A.GetAbs();
	}

	template< typename T >
	UE::Math::TVector4<T> Abs( const UE::Math::TVector4<T>& A )
	{
		return UE::Math::TVector4<T>(
			FMath::Abs( A.X ),
			FMath::Abs( A.Y ),
			FMath::Abs( A.Z ),
			FMath::Abs( A.W ) );
	}
}

// [ Thonat et al. 2021, "Tessellation-Free Displacement Mapping for Ray Tracing" ]
// [ de Figueiredo and Stolf 2004, "Affine Arithmetic: Concepts and Applications" ]
// [ Rump and Kashiwagi 2015, "Implementation and improvements of affine arithmetic" ]

template< typename T, uint32 Num >
struct TAffine
{
	T c;
	T K;
	T e[ Num ];

	TAffine() {}
	FORCEINLINE TAffine( T Constant )
		: c( Constant )
		, K( 0.0f )
	{
		for( uint32 i = 0; i < Num; i++ )
			e[i] = T( 0.0f );
	}

	FORCEINLINE TAffine( T Min, T Max )
		: c( 0.5f * ( Min + Max ) )
		, K( 0.5f * ( Max - Min ) )
	{
		for( uint32 i = 0; i < Num; i++ )
			e[i] = T( 0.0f );
	}

	FORCEINLINE TAffine( T Min, T Max, uint32 Index )
		: c( 0.5f * ( Min + Max ) )
		, K( 0.0f )
	{
		for( uint32 i = 0; i < Num; i++ )
			e[i] = T( 0.0f );
		e[ Index ] = 0.5f * ( Max - Min );
	}

	FORCEINLINE TAffine< T, Num >& operator+=( const TAffine< T, Num >& Other )
	{
		c += Other.c;
		K += Other.K;
		for( uint32 i = 0; i < Num; i++ )
			e[i] += Other.e[i];
		
		return *this;
	}

	FORCEINLINE TAffine< T, Num > operator+( const TAffine< T, Num >& Other ) const
	{
		return TAffine< T, Num >(*this) += Other;
	}

	FORCEINLINE TAffine< T, Num >& operator-=( const TAffine< T, Num >& Other )
	{
		c -= Other.c;
		K += Other.K;
		for( uint32 i = 0; i < Num; i++ )
			e[i] -= Other.e[i];
		
		return *this;
	}

	FORCEINLINE TAffine< T, Num > operator-( const TAffine< T, Num >& Other ) const
	{
		return TAffine< T, Num >(*this) -= Other;
	}

	FORCEINLINE T GetMin() const
	{
		using namespace AbsHelper;

		T Result = c - Abs(K);
		for( uint32 i = 0; i < Num; i++ )
			Result -= Abs( e[i] );

		return Result;
	}

	FORCEINLINE T GetMax() const
	{
		using namespace AbsHelper;
		
		T Result = c + Abs(K);
		for( uint32 i = 0; i < Num; i++ )
			Result += Abs( e[i] );

		return Result;
	}

	// Smaller than (v|v)
	FORCEINLINE TAffine< float, Num > SizeSquared() const
	{
		using namespace AbsHelper;
		
		TAffine< float, Num > Result;
		Result.c = c.SizeSquared();
		Result.K = 2.0f * Abs( c | K );

		T Extent = K;
		for( uint32 i = 0; i < Num; i++ )
		{
			Extent += Abs( e[i] );
			Result.e[i] = 2.0f * ( c | e[i] );
		}

		Result.c += 0.5f * Extent.SizeSquared();
		Result.K += 0.5f * Extent.SizeSquared();

		return Result;
	}
};

template< typename T, typename U, uint32 Num >
FORCEINLINE TAffine< T, Num > operator*( const TAffine< T, Num >& A, const TAffine< U, Num >& B )
{
	using namespace AbsHelper;

	TAffine< T, Num > Result;
	Result.c = A.c * B.c;
	Result.K =
		Abs( A.K * B.c ) +
		Abs( A.c * B.K );

	T AK = A.K;
	U BK = B.K;
	for( uint32 i = 0; i < Num; i++ )
	{
		Result.e[i] = A.e[i] * B.c + A.c * B.e[i];
		AK += Abs( A.e[i] );
		BK += Abs( B.e[i] );
	}
	Result.K += AK * BK;

	return Result;
}

template< typename T, uint32 Num >
FORCEINLINE TAffine< float, Num > operator|( const TAffine< T, Num >& A, const TAffine< T, Num >& B )
{
	using namespace AbsHelper;

	TAffine< float, Num > Result;
	Result.c = A.c | B.c;
	Result.K =
		Abs( A.K | B.c ) +
		Abs( A.c | B.K );

	T AK = A.K;
	T BK = B.K;
	for( uint32 i = 0; i < Num; i++ )
	{
		Result.e[i] = ( A.e[i] | B.c ) + ( A.c | B.e[i] );
		AK += Abs( A.e[i] );
		BK += Abs( B.e[i] );
	}
	Result.K += AK | BK;

	return Result;
}

template< uint32 Num >
FORCEINLINE TAffine< float, Num > Clamp( const TAffine< float, Num >& x, float Min, float Max )
{
	// Using Chebyshev approximation
	float xMin = x.GetMin();
	float xMax = x.GetMax();
	float FuncMin = FMath::Clamp( xMin, Min, Max );
	float FuncMax = FMath::Clamp( xMax, Min, Max );

	if( Min <= xMin && xMax <= Max )
		return x;
	if( xMax <= Min )
		return TAffine< float, Num >( Min );
	if( xMin >= Max )
		return TAffine< float, Num >( Max );

	float Alpha = ( FuncMax - FuncMin ) / ( xMax - xMin );
	float Gamma = 0.5f * ( 1.0f - Alpha ) * ( FuncMax + FuncMin );
	float Delta = ( 1.0f - Alpha ) * FuncMax - Gamma;

	TAffine< float, Num > Result;
	Result.c = Alpha * x.c + Gamma;
	Result.K = FMath::Abs( Alpha * x.K ) + Delta;
	for( uint32 i = 0; i < Num; i++ )
		Result.e[i] = Alpha * x.e[i];
	
	return Result;
}

template< uint32 Num >
FORCEINLINE TAffine< float, Num > InvSqrt( const TAffine< float, Num >& x )
{
	// Using min range approximation
	float xMin = FMath::Max( 1e-4f, x.GetMin() );
	float xMax = FMath::Max( 1e-4f, x.GetMax() );
	float FuncMin = FMath::InvSqrt( xMin );
	float FuncMax = FMath::InvSqrt( xMax );

	float Alpha = -0.5f * FuncMax * FuncMax * FuncMax;
	float Gamma = 0.5f * ( FuncMin + FuncMax - Alpha * ( xMin + xMax ) );
	float Delta = FMath::Abs( 0.5f * ( FuncMin - FuncMax - Alpha * ( xMin - xMax ) ) );

	TAffine< float, Num > Result;
	Result.c = Alpha * x.c + Gamma;
	Result.K = FMath::Abs( Alpha * x.K ) + Delta;
	for( uint32 i = 0; i < Num; i++ )
		Result.e[i] = Alpha * x.e[i];

	return Result;
}

template< typename T, uint32 Num >
FORCEINLINE TAffine< T, Num > Normalize( const TAffine< T, Num >& x )
{
	return x * InvSqrt( Clamp( x.SizeSquared(), 1e-4f, 1.0f ) );
}