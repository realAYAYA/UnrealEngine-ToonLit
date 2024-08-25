// Copyright Epic Games, Inc. All Rights Reserved.

// Port of geometry3Sharp DenseGrid3

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
 * 3D dense grid of floating-point scalar values. 
 */
template<typename ElemType>
class TDenseGrid3
{
protected:
	/** grid of allocated elements */
	TArray<ElemType> Buffer;

	/** dimensions per axis */
	FVector3i Dimensions;

public:
	/**
	 * Create empty grid
	 */
	TDenseGrid3() : Dimensions(0,0,0)
	{
	}

	TDenseGrid3(int DimX, int DimY, int DimZ, ElemType InitialValue)
	{
		Resize(DimX, DimY, DimZ);
		Assign(InitialValue);
	}

	int Size() const
	{
		return Dimensions.X * Dimensions.Y * Dimensions.Z;
	}

	bool IsValidIndex(const FVector3i& Index) const
	{
		return Index.X >= 0 && Index.Y >= 0 && Index.Z >= 0
			&& Index.X < Dimensions.X && Index.Y < Dimensions.Y && Index.Z < Dimensions.Z;
	}

	const FVector3i& GetDimensions() const
	{
		return Dimensions;
	}

	void Resize(int DimX, int DimY, int DimZ, EAllowShrinking AllowShrinking = EAllowShrinking::Yes)
	{
		check((int64)DimX * (int64)DimY * (int64)DimZ < INT_MAX);
		Buffer.SetNumUninitialized(DimX * DimY * DimZ, AllowShrinking);
		Dimensions = FVector3i(DimX, DimY, DimZ);
	}
	UE_ALLOWSHRINKING_BOOL_DEPRECATED("Resize")
	FORCEINLINE void Resize(int DimX, int DimY, int DimZ, bool bAllowShrinking)
	{
		Resize(DimX, DimY, DimZ, bAllowShrinking ? EAllowShrinking::Yes : EAllowShrinking::No);
	}

	void Assign(ElemType Value)
	{
		for (int i = 0; i < Buffer.Num(); ++i)
		{
			Buffer[i] = Value;
		}
	}

	void SetMin(const FVector3i& IJK, ElemType F)
	{
		int Idx = IJK.X + Dimensions.X * (IJK.Y + Dimensions.Y * IJK.Z);
		if (F < Buffer[Idx])
		{
			Buffer[Idx] = F;
		}
	}
	void SetMax(const FVector3i& IJK, ElemType F)
	{
		int Idx = IJK.X + Dimensions.X * (IJK.Y + Dimensions.Y * IJK.Z);
		if (F > Buffer[Idx])
		{
			Buffer[Idx] = F;
		}
	}

	constexpr ElemType& operator[](int Idx)
	{
		return Buffer[Idx];
	}
	constexpr const ElemType& operator[](int Idx) const
	{
		return Buffer[Idx];
	}

	constexpr ElemType& operator[](FVector3i Idx)
	{
		return Buffer[Idx.X + Dimensions.X * (Idx.Y + Dimensions.Y * Idx.Z)];
	}
	constexpr const ElemType& operator[](FVector3i Idx) const
	{
		return Buffer[Idx.X + Dimensions.X * (Idx.Y + Dimensions.Y * Idx.Z)];
	}
	constexpr ElemType& At(int I, int J, int K)
	{
		return Buffer[I + Dimensions.X * (J + Dimensions.Y * K)];
	}
	constexpr const ElemType& At(int I, int J, int K) const
	{
		return Buffer[I + Dimensions.X * (J + Dimensions.Y * K)];
	}

	TArray<ElemType>& GridValues()
	{
		return Buffer;
	}
	const TArray<ElemType>& GridValues() const
	{
		return Buffer;
	}

	/**
	* @return the grid value at (X,Y,Z)
	*/
	ElemType GetValue(int32 X, int32 Y, int32 Z) const
	{
		return Buffer[X + Dimensions.X * (Y + Dimensions.Y * Z)];
	}

	/**
	* @return the grid value at (X,Y,Z)
	*/
	ElemType GetValue(const FVector3i& CellCoords) const
	{
		return Buffer[CellCoords.X + Dimensions.X * (CellCoords.Y + Dimensions.Y * CellCoords.Z)];
	}

	/**
	* Set the grid value at (X,Y,Z)
	*/
	void SetValue(int32 X, int32 Y, int32 Z, ElemType NewValue)
	{
		Buffer[X + Dimensions.X * (Y + Dimensions.Y * Z)] = NewValue;
	}

	/**
	* Set the grid value at (X,Y,Z)
	*/
	void SetValue(const FVector3i& CellCoords, ElemType NewValue)
	{
		Buffer[CellCoords.X + Dimensions.X * (CellCoords.Y + Dimensions.Y * CellCoords.Z)] = NewValue;
	}

	/**
	* Call an external lambda with a reference to the grid value at (X,Y,Z).
	* Called as Func(ElemType&), so the caller can both read and write the grid cell
	*/
	template<typename ProcessFunc>
	void ProcessValue(int32 X, int32 Y, int32 Z, ProcessFunc Func)
	{
		int32 Index = X + Dimensions.X * (Y + Dimensions.Y * Z);
		ElemType& CurValue = Buffer[Index];
		Func(CurValue);
	}

	/**
	* Call an external lambda with a reference to the grid value at (X,Y,Z).
	* Called as Func(ElemType&), so the caller can both read and write the grid cell
	*/
	template<typename ProcessFunc>
	void ProcessValue(const FVector3i& CellCoords, ProcessFunc Func)
	{
		int32 Index = CellCoords.X + Dimensions.X * (CellCoords.Y + Dimensions.Y * CellCoords.Z);
		ElemType& CurValue = Buffer[Index];
		Func(CurValue);
	}



	void GetXPair(int X0, int Y, int Z, ElemType& AOut, ElemType& BOut) const
	{
		int Offset = Dimensions.X * (Y + Dimensions.Y * Z);
		AOut = Buffer[Offset + X0];
		BOut = Buffer[Offset + X0 + 1];
	}

	void Apply(TFunctionRef<ElemType(ElemType)> F)
	{
		for (int Idx = 0, Num = Size(); Idx < Num; Idx++)
		{
			Buffer[Idx] = F(Buffer[Idx]);
		}
	}

	FAxisAlignedBox3i Bounds() const
	{
		return FAxisAlignedBox3i({0, 0, 0},{Dimensions.X, Dimensions.Y, Dimensions.Z});
	}
	FAxisAlignedBox3i BoundsInclusive() const
	{
		return FAxisAlignedBox3i({0, 0, 0},{Dimensions.X-1, Dimensions.Y-1, Dimensions.Z-1});
	}

	FVector3i ToIndex(int Idx) const
	{
		int x = Idx % Dimensions.X;
		int y = (Idx / Dimensions.X) % Dimensions.Y;
		int z = Idx / (Dimensions.X * Dimensions.Y);
		return FVector3i(x, y, z);
	}
	int ToLinear(int X, int Y, int Z) const
	{
		return X + Dimensions.X * (Y + Dimensions.Y * Z);
	}
	int ToLinear(const FVector3i& IJK) const
	{
		return IJK.X + Dimensions.X * (IJK.Y + Dimensions.Y * IJK.Z);
	}
};


typedef TDenseGrid3<float> FDenseGrid3f;
typedef TDenseGrid3<double> FDenseGrid3d;
typedef TDenseGrid3<int> FDenseGrid3i;

// additional utility functions
namespace DenseGrid
{

	inline void AtomicIncrement(FDenseGrid3i& Grid, int i, int j, int k)
	{
		FPlatformAtomics::InterlockedIncrement(&Grid.At(i,j,k));
	}

	inline void AtomicDecrement(FDenseGrid3i& Grid, int i, int j, int k)
	{
		FPlatformAtomics::InterlockedDecrement(&Grid.At(i, j, k));
	}

	inline void AtomicIncDec(FDenseGrid3i& Grid, int i, int j, int k, bool bDecrement = false)
	{
		if (bDecrement)
		{
			FPlatformAtomics::InterlockedDecrement(&Grid.At(i, j, k));
		}
		else
		{
			FPlatformAtomics::InterlockedIncrement(&Grid.At(i, j, k));
		}
	}
}

} // end namespace UE::Geometry
} // end namespace UE
