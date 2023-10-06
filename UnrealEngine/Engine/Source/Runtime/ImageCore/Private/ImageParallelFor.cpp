// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImageParallelFor.h"

/**

FImage is tight packed in memory with slices adjacent to each other
so we can just treat it was a 2d image with Height *= NumSlices
 
*/

IMAGECORE_API int32 FImageCore::ImageParallelForComputeNumJobs(const FImageView & Image,int64 * pRowsPerJob)
{
	int64 SizeX = Image.SizeX;
	int64 SizeY = (int64)Image.SizeY * Image.NumSlices;

	int32 RowsPerJob;
	int32 NumJobs = ImageParallelForComputeNumJobsForRows(RowsPerJob,SizeX,SizeY);

	check( (int64)NumJobs * RowsPerJob >= SizeY );
	check( (int64)(NumJobs-1) * RowsPerJob < SizeY );

	*pRowsPerJob = RowsPerJob;
	return NumJobs;
}

IMAGECORE_API void FImageCore::ImageParallelForMakePart(FImageView * Part,const FImageView & Image,int64 JobIndex,int64 RowsPerJob)
{
	int64 SizeX = Image.SizeX;
	int64 SizeY = (int64)Image.SizeY * Image.NumSlices;

	int64 StartY = JobIndex * RowsPerJob;
	check( StartY < SizeY );

	int64 EndY = FMath::Min(StartY + RowsPerJob,SizeY);

	*Part = Image;
	Part->SizeY = EndY - StartY;
	Part->NumSlices = 1;
	Part->RawData = (uint8 *)Image.RawData + Image.GetBytesPerPixel() * SizeX * StartY;
}

namespace
{

struct FLinearColorCmp
{
	inline bool operator () (const FLinearColor & lhs,const FLinearColor & rhs) const
	{
		if ( lhs.R != rhs.R ) return lhs.R < rhs.R;
		if ( lhs.G != rhs.G ) return lhs.G < rhs.G;
		if ( lhs.B != rhs.B ) return lhs.B < rhs.B;
		if ( lhs.A != rhs.A ) return lhs.A < rhs.A;
		return false;
	}
};

static inline FLinearColor SumColors(const TArrayView64<FLinearColor> & Colors)
{
	VectorRegister4Float VecSum = VectorSetFloat1(0.f);
	for ( FLinearColor & Color : Colors )
	{
		VecSum = VectorAdd(VecSum, VectorLoad(&Color.Component(0)));
	}

	FLinearColor Sum;
	VectorStore(VecSum,&Sum.Component(0));
	
	return Sum;
}

};

IMAGECORE_API FLinearColor FImageCore::ComputeImageLinearAverage(const FImageView & Image)
{
	TArray<FLinearColor> Accumulator_Rows;
	int64 Accumulator_RowCount = (int64)Image.SizeY*Image.NumSlices;
	int64 Accumulator_RowIndex = 0;
	Accumulator_Rows.SetNum(Accumulator_RowCount);
	
	// just summing parallel portions to an accumulator would produce different output depending on thread count and timing
	//	because the float sums to accumulator are not order and grouping invariant
	// instead we are careful to ensure machine invariance
	// the image is cut into rows
	// each row is summed
	// then all those row sums are sorted
	// so they are accumulated in a fixed order

	ImageParallelProcessLinearPixels(TEXT("PF.ComputeImageLinearAverage"),Image,
		[&](TArrayView64<FLinearColor> & Colors) 
		{
			// this is called once per row
			//	so it is always the same grouping of colors regardless of thread count
			
			FLinearColor Sum = SumColors(Colors);

			// do not just += on accumulator here because that would be an order-dependent race that changes output
			// instead we store all the row sums to later accumulate in known order

			int64 Index = FPlatformAtomics::InterlockedAdd(&Accumulator_RowIndex, 1);
			Accumulator_Rows[Index] = Sum;

			return ProcessLinearPixelsAction::ReadOnly;
		}
	);

	check( Accumulator_RowIndex == Accumulator_RowCount );

	Accumulator_Rows.Sort(FLinearColorCmp());

	FLinearColor Accumulator = SumColors(Accumulator_Rows);

	int64 NumPixels = Image.GetNumPixels();
	Accumulator *= (1.f / NumPixels);

	return Accumulator;
}