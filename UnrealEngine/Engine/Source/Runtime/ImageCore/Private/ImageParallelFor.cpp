// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImageParallelFor.h"

/**

FImage is tight packed in memory with slices adjacent to each other
so we can just treat it was a 2d image with Height *= NumSlices

@todo Oodle : actually because of the tight packed property, there's no need to use the original image dimensions at all
we could just cut into 1d runs of the desired pixel count.
That would give better parallelism on skew images (than keeping original SizeX which we do now).

eg. make "ImagePart" of 16384 pixels, and make "Rows" for the FLinearColor pass that are always exactly 512 pixels

*/

IMAGECORE_API int32 FImageCore::ImageParallelForComputeNumJobs(const FImageView & Image,int64 * pRowsPerJob)
{
	int64 SizeX = Image.SizeX;
	int64 SizeY = ImageParallelForComputeNumRows(Image);

	int32 RowsPerJob;
	int32 NumJobs = ImageParallelForComputeNumJobsForRows(RowsPerJob,SizeX,SizeY);

	check( (int64)NumJobs * RowsPerJob >= SizeY );
	check( (int64)(NumJobs-1) * RowsPerJob < SizeY );

	*pRowsPerJob = RowsPerJob;
	return NumJobs;
}

IMAGECORE_API int64 FImageCore::ImageParallelForMakePart(FImageView * Part,const FImageView & Image,int64 JobIndex,int64 RowsPerJob)
{
	int64 SizeX = Image.SizeX;
	int64 SizeY = ImageParallelForComputeNumRows(Image);

	int64 StartY = JobIndex * RowsPerJob;
	check( StartY < SizeY );

	int64 EndY = FMath::Min(StartY + RowsPerJob,SizeY);

	*Part = Image;
	Part->SizeY = EndY - StartY;
	Part->NumSlices = 1;
	Part->RawData = (uint8 *)Image.RawData + Image.GetBytesPerPixel() * SizeX * StartY;

	return StartY;
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
	int64 Accumulator_RowCount = ImageParallelForComputeNumRows(Image);
	Accumulator_Rows.SetNum(Accumulator_RowCount);
	
	// just summing parallel portions to an accumulator would produce different output depending on thread count and timing
	//	because the float sums to accumulator are not order and grouping invariant
	// instead we are careful to ensure machine invariance
	// the image is cut into rows
	// each row is summed
	// then all those row sums are accumulated

	ImageParallelProcessLinearPixels(TEXT("PF.ComputeImageLinearAverage"),Image,
		[&](TArrayView64<FLinearColor> & Colors,int64 Y) 
		{
			// this is called once per row
			//	so it is always the same grouping of colors regardless of thread count
			
			FLinearColor Sum = SumColors(Colors);

			// do not just += on accumulator here because that would be an order-dependent race that changes output
			// instead we store all the row sums to later accumulate in known order

			Accumulator_Rows[Y] = Sum;

			return ProcessLinearPixelsAction::ReadOnly;
		}
	);

	FLinearColor Accumulator = SumColors(Accumulator_Rows);

	int64 NumPixels = Image.GetNumPixels();
	Accumulator *= (1.f / NumPixels);

	return Accumulator;
}

namespace
{

	struct FMinMax
	{
		VectorRegister4Float VMin,VMax;

		FMinMax() { }

		FMinMax(VectorRegister4Float InMin,VectorRegister4Float InMax) : VMin(InMin), VMax(InMax)
		{
		}
	};
	
	static inline FMinMax MinMax(const FMinMax & A,const FMinMax & B)
	{
		return FMinMax(
			VectorMin(A.VMin,B.VMin),
			VectorMax(A.VMax,B.VMax) );
	}

	static inline FMinMax MinMaxColors(const TArrayView64<FLinearColor> & Colors)
	{
		check( Colors.Num() > 0 );

		VectorRegister4Float VMin = VectorLoad(&Colors[0].Component(0));
		VectorRegister4Float VMax = VMin;
	
		for ( const FLinearColor & Color : Colors )
		{
			VectorRegister4Float VCur = VectorLoad(&Color.Component(0));

			VMin = VectorMin(VMin,VCur);
			VMax = VectorMax(VMax,VCur);
		}

		return FMinMax(VMin,VMax);
	}

};

void FImageCore::ComputeChannelLinearMinMax(const FImageView & Image, FLinearColor & OutMin, FLinearColor & OutMax)
{
	if ( Image.GetNumPixels() == 0 )
	{
		OutMin = FLinearColor(ForceInit);
		OutMax = FLinearColor(ForceInit);
		return;
	}
	
	TArray<FMinMax> MinMax_Rows;
	int64 MinMax_RowCount = ImageParallelForComputeNumRows(Image);
	MinMax_Rows.SetNum(MinMax_RowCount);
	
	ImageParallelProcessLinearPixels(TEXT("PF.ComputeChannelLinearMinMax"),Image,
		[&](TArrayView64<FLinearColor> & Colors,int64 Y) 
		{
			FMinMax Sum = MinMaxColors(Colors);

			MinMax_Rows[Y] = Sum;

			return ProcessLinearPixelsAction::ReadOnly;
		}
	);
	
	// now MinMax on all the rows :
	FMinMax NetMinMax = MinMax_Rows[0];
	for (const FMinMax & MM_Row : MinMax_Rows )
	{
		NetMinMax = MinMax(NetMinMax, MM_Row );
	}

	VectorStore(NetMinMax.VMin,&OutMin.Component(0));
	VectorStore(NetMinMax.VMax,&OutMax.Component(0));
}

bool FImageCore::ScaleChannelsSoMinMaxIsInZeroToOne(const FImageView & Image)
{
	if ( Image.GetNumPixels() == 0 )
	{
		return false;
	}
	if ( ! ERawImageFormat::IsHDR(Image.Format) )
	{
		// early out : if Image is U8/U16 it is already in [0,1]
		return false;
	}

	FLinearColor Min,Max;
	ComputeChannelLinearMinMax(Image,Min,Max);

	if ( Min.R >= 0.f && Min.G >= 0.f && Min.B >= 0.f && Min.A >= 0.f &&
		Max.R <= 1.f && Max.G <= 1.f && Max.B <= 1.f && Max.A <= 1.f )
	{
		// nothing to do
		return false;
	}

	VectorRegister4Float VMin = VectorLoad(&Min.Component(0));
	VectorRegister4Float VMax = VectorLoad(&Max.Component(0));

	// this makes it so that the end of the range that was already in [0,1] is not modified :
	VMin = VectorMin( VMin, MakeVectorRegisterFloat(0.f,0.f,0.f,0.f) );
	VMax = VectorMax( VMax, MakeVectorRegisterFloat(1.f,1.f,1.f,1.f) );

	// VScale = 1.f/(Max-Min)
	VectorRegister4Float VSub = VectorSubtract(VMax,VMin);
	// avoid divide by zero :
	VSub = VectorMax(VSub, MakeVectorRegisterFloat(FLT_MIN,FLT_MIN,FLT_MIN,FLT_MIN) );
	VectorRegister4Float VScale = VectorReciprocalAccurate(VSub);

	ImageParallelProcessLinearPixels(TEXT("PF.ScaleChannelsSoMinMaxIsInZeroToOne"),Image,
		[&](TArrayView64<FLinearColor> & Colors,int64 Y) 
		{
			for ( FLinearColor & Color : Colors )
			{
				VectorRegister4Float VCur = VectorLoad(&Color.Component(0));

				VCur = VectorSubtract(VCur,VMin);
				VCur = VectorMultiply(VCur,VScale);

				VectorStore(VCur, &Color.Component(0));
			}

			return ProcessLinearPixelsAction::Modified;
		}
	);

	return true;
}
