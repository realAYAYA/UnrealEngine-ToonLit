// Copyright Epic Games, Inc. All Rights Reserved.

// Port of geometry3Sharp DenseGrid2

#pragma once

#include "CoreMinimal.h"
#include "BoxTypes.h"
#include "IntBoxTypes.h"

#include "HAL/PlatformAtomics.h"

namespace UE
{
namespace Geometry
{

/**
 * 2D dense grid of scalar values.
 */
template<typename ElemType>
class TDenseGrid2
{
protected:
	/** grid of allocated elements */
	TArray64<ElemType> Buffer;

	/** dimensions per axis */
	int64 DimensionX, DimensionY;

public:
	/**
	 * Create empty grid
	 */
	TDenseGrid2() : DimensionX(0), DimensionY(0)
	{
	}

	TDenseGrid2(int32 DimX, int32 DimY, ElemType InitialValue)
	{
		DimensionX = (int64)DimX;
		DimensionY = (int64)DimY;
		Buffer.Init(InitialValue, Size());
	}

	int64 Size() const
	{
		return DimensionX * DimensionY;
	}

	int64 Width() const
	{
		return DimensionX;
	}

	int64 Height() const
	{
		return DimensionY;
	}

	FVector2i GetDimensions() const
	{
		return FVector2i((int32)DimensionX, (int32)DimensionY);
	}

	void Resize(int32 DimX, int32 DimY, EAllowShrinking AllowShrinking = EAllowShrinking::Yes)
	{
		DimensionX = (int64)DimX;
		DimensionY = (int64)DimY;
		Buffer.SetNumUninitialized( DimensionX*DimensionY, AllowShrinking);
	}
	UE_ALLOWSHRINKING_BOOL_DEPRECATED("Resize")
	FORCEINLINE void Resize(int32 DimX, int32 DimY, bool bAllowShrinking)
	{
		Resize(DimX, DimY, bAllowShrinking ? EAllowShrinking::Yes : EAllowShrinking::No);
	}

	void AssignAll(ElemType Value)
	{
		for (int64 i = 0, Num = Size(); i < Num; ++i)
		{
			Buffer[i] = Value;
		}
	}

	void SetMin(const FVector2i& IJK, ElemType NewValue)
	{
		int64 Idx = (int64)IJK.X + (DimensionX * (int64)IJK.Y);
		if (NewValue < Buffer[Idx])
		{
			Buffer[Idx] = NewValue;
		}
	}

	void SetMax(const FVector2i& IJK, ElemType NewValue)
	{
		int64 Idx = (int64)IJK.X + (DimensionX * (int64)IJK.Y);
		if (NewValue > Buffer[Idx])
		{
			Buffer[Idx] = NewValue;
		}
	}

	constexpr ElemType& operator[](int64 Idx)
	{
		return Buffer[Idx];
	}
	constexpr const ElemType& operator[](int64 Idx) const
	{
		return Buffer[Idx];
	}

	constexpr ElemType& operator[](FVector2i Idx)
	{
		return Buffer[(int64)Idx.X + DimensionX * (int64)Idx.Y];
	}
	constexpr const ElemType& operator[](FVector2i Idx) const
	{
		return Buffer[(int64)Idx.X + DimensionX * (int64)Idx.Y];
	}
	constexpr ElemType& At(int32 X, int32 Y)
	{
		return Buffer[(int64)X + DimensionX * (int64)Y];
	}
	constexpr const ElemType& At(int32 X, int32 Y) const
	{
		return Buffer[(int64)X + DimensionX * (int64)Y];
	}
	constexpr ElemType& At(int64 X, int64 Y)
	{
		return Buffer[X + DimensionX * Y];
	}
	constexpr const ElemType& At(int64 X, int64 Y) const
	{
		return Buffer[X + DimensionX * Y];
	}

	TArray64<ElemType>& GridValues()
	{
		return Buffer;
	}
	const TArray64<ElemType>& GridValues() const
	{
		return Buffer;
	}


	void GetXPair(int32 X0, int32 Y, ElemType& AOut, ElemType& BOut) const
	{
		int64 Offset = DimensionX * (int64)Y;
		AOut = Buffer[Offset + X0];
		BOut = Buffer[Offset + X0 + 1];
	}

	void Apply(TFunctionRef<ElemType(ElemType)> ApplyFunc)
	{
		for (int64 Idx = 0, Num = Size(); Idx < Num; Idx++)
		{
			Buffer[Idx] = F(Buffer[Idx]);
		}
	}

	FAxisAlignedBox2i Bounds() const
	{
		return FAxisAlignedBox2i({ 0, 0}, { (int32)DimensionX, (int32)DimensionY});
	}
	FAxisAlignedBox2i BoundsInclusive() const
	{
		return FAxisAlignedBox2i({ 0, 0}, { (int32)(DimensionX - 1), (int32)(DimensionY - 1)});
	}

	FVector2i GetCoords(int64 LinearIndex) const
	{
		checkSlow(LinearIndex >= 0);
		return FVector2i((int32)(LinearIndex % (int64)DimensionX), (int32)(LinearIndex / (int64)DimensionX));
	}
	int64 GetIndex(int32 X, int32 Y) const
	{
		return X + DimensionX * Y;
	}
	int64 GetIndex(const FVector2i& IJK) const
	{
		return IJK.X + DimensionX * IJK.Y;
	}
};


typedef TDenseGrid2<float> FDenseGrid2f;
typedef TDenseGrid2<double> FDenseGrid2d;
typedef TDenseGrid2<int32> FDenseGrid2i;


// additional utility functions
namespace DenseGrid
{

	inline void AtomicIncrement(FDenseGrid2i& Grid, int32 i, int32 j)
	{
		FPlatformAtomics::InterlockedIncrement(&Grid.At(i, j));
	}

	inline void AtomicDecrement(FDenseGrid2i& Grid, int32 i, int32 j)
	{
		FPlatformAtomics::InterlockedDecrement(&Grid.At(i, j));
	}

	inline void AtomicIncDec(FDenseGrid2i& Grid, int32 i, int32 j, bool bDecrement = false)
	{
		if (bDecrement)
		{
			FPlatformAtomics::InterlockedDecrement(&Grid.At(i, j));
		}
		else
		{
			FPlatformAtomics::InterlockedIncrement(&Grid.At(i, j));
		}
	}
}

} // end namespace UE::Geometry
} // end namespace UE
