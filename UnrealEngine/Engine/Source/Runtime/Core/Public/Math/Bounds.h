// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Math/UnrealMathUtility.h"
#include "Math/Vector.h"
#include "Math/Vector4.h"

template< typename T >
struct TBounds
{
	template< typename U > using TVector  = UE::Math::TVector<U>;
	template< typename U > using TVector4 = UE::Math::TVector4<U>;
	using FReal = T;

	TVector4<T>	Min = TVector4<T>(  TNumericLimits<T>::Max(),  TNumericLimits<T>::Max(),  TNumericLimits<T>::Max() );
	TVector4<T>	Max = TVector4<T>( -TNumericLimits<T>::Max(), -TNumericLimits<T>::Max(), -TNumericLimits<T>::Max() );

	FORCEINLINE TBounds<T>& operator=( const TVector<T>& Other )
	{
		Min = Other;
		Max = Other;
		return *this;
	}

	FORCEINLINE TBounds<T>& operator+=( const TVector<T>& Other )
	{
		VectorStoreAligned( VectorMin( VectorLoadAligned( &Min ), VectorLoadFloat3( &Other ) ), &Min );
		VectorStoreAligned( VectorMax( VectorLoadAligned( &Max ), VectorLoadFloat3( &Other ) ), &Max );
		return *this;
	}

	FORCEINLINE TBounds<T>& operator+=( const TBounds<T>& Other )
	{
		VectorStoreAligned( VectorMin( VectorLoadAligned( &Min ), VectorLoadAligned( &Other.Min ) ), &Min );
		VectorStoreAligned( VectorMax( VectorLoadAligned( &Max ), VectorLoadAligned( &Other.Max ) ), &Max );
		return *this;
	}

	FORCEINLINE TBounds<T> operator+( const TBounds<T>& Other ) const
	{
		return TBounds<T>(*this) += Other;
	}

	FORCEINLINE bool Intersect( const TBounds<T>& Other ) const
	{
		int Separated;
		Separated  = VectorAnyGreaterThan( VectorLoadAligned( &Min ), VectorLoadAligned( &Other.Max ) );
		Separated |= VectorAnyGreaterThan( VectorLoadAligned( &Other.Min ), VectorLoadAligned( &Max ) );
		Separated &= 0b111;
		return Separated == 0;
	}

	FORCEINLINE bool Contains( const TBounds<T>& Other ) const
	{
		int MaskMin = VectorMaskBits( VectorCompareLE( VectorLoadAligned( &Min ), VectorLoadAligned( &Other.Min ) ) );
		int MaskMax = VectorMaskBits( VectorCompareGE( VectorLoadAligned( &Max ), VectorLoadAligned( &Other.Max ) ) );
		return ( MaskMin & MaskMax & 7 ) == 7;
	}

	FORCEINLINE T DistSqr( const TVector<T>& Point ) const
	{
		auto rMin		= VectorLoadAligned( &Min );
		auto rMax		= VectorLoadAligned( &Max );
		auto rPoint		= VectorLoadFloat3( &Point );
		auto rClosest	= VectorSubtract( VectorMin( VectorMax( rPoint, rMin ), rMax ), rPoint );
		return VectorDot3Scalar( rClosest, rClosest );
	}

	FORCEINLINE TVector<T> GetCenter() const
	{
		return (Max + Min) * 0.5f;
	}

	FORCEINLINE TVector<T> GetExtent() const
	{
		return (Max - Min) * 0.5f;
	}

	FORCEINLINE TVector<T> GetSize() const
	{
		return (Max - Min);
	}

	FORCEINLINE T GetSurfaceArea() const
	{
		TVector<T> Size = Max - Min;
		return 0.5f * (Size.X * Size.Y + Size.X * Size.Z + Size.Y * Size.Z);
	}

	template< typename U >
	FORCEINLINE auto ToAbsolute( const TVector<U>& Offset ) const
	{
		using CommonType = typename std::common_type<T, U>::type;

		TBounds< CommonType > Bounds;
		Bounds.Min = TVector4< CommonType >( Min ) + TVector4< CommonType >( Offset, 0.0 );
		Bounds.Max = TVector4< CommonType >( Max ) + TVector4< CommonType >( Offset, 0.0 );
		return Bounds;
	}

	template< typename U >
	FORCEINLINE TBounds< float > ToRelative( const TVector<U>& Offset ) const
	{
		using CommonType = typename std::common_type<T, U>::type;

		TBounds< float > Bounds;
		Bounds.Min = TVector4< float >( TVector4< CommonType >( Min ) - TVector4< CommonType >( Offset, 0.0 ) );
		Bounds.Max = TVector4< float >( TVector4< CommonType >( Max ) - TVector4< CommonType >( Offset, 0.0 ) );
		return Bounds;
	}

	FORCEINLINE friend FArchive& operator<<( FArchive& Ar, TBounds<T>& Bounds )
	{
		Ar << Bounds.Min;
		Ar << Bounds.Max;
		return Ar;
	}
};

using FBounds3f = TBounds< float >;
using FBounds3d = TBounds< double >;