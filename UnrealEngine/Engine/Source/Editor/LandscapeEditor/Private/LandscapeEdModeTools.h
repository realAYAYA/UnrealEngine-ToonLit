// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/GCObject.h"
#include "LandscapeProxy.h"
#include "LandscapeToolInterface.h"
#include "LandscapeEdMode.h"
#include "LandscapeEditTypes.h"
#include "EditorViewportClient.h"
#include "LandscapeEdit.h"
#include "LandscapeComponent.h"
#include "LandscapeDataAccess.h"
#include "LandscapeHeightfieldCollisionComponent.h"
#include "InstancedFoliageActor.h"
#include "AI/NavigationSystemBase.h"
#include "Landscape.h"
#include "LandscapeEditorPrivate.h"
#include "Logging/LogMacros.h"
#include "VisualLogger/VisualLogger.h"


//
//	FNoiseParameter - Perlin noise
//
struct FNoiseParameter
{
	float Base;
	float NoiseScale;
	float NoiseAmount;

	// Constructors.

	FNoiseParameter()
	{
	}
	FNoiseParameter(float InBase, float InScale, float InAmount) :
		Base(InBase),
		NoiseScale(InScale),
		NoiseAmount(InAmount)
	{
	}

	// Sample
	float Sample(int32 X, int32 Y) const
	{
		float	Noise = 0.0f;
		X = FMath::Abs(X);
		Y = FMath::Abs(Y);

		if (NoiseScale > DELTA)
		{
			for (uint32 Octave = 0; Octave < 4; Octave++)
			{
				float	OctaveShift = static_cast<float>(1 << Octave);
				float	OctaveScale = OctaveShift / NoiseScale;
				Noise += PerlinNoise2D(X * OctaveScale, Y * OctaveScale) / OctaveShift;
			}
		}

		return Base + Noise * NoiseAmount;
	}

	// TestGreater - Returns 1 if TestValue is greater than the parameter.
	bool TestGreater(int32 X, int32 Y, float TestValue) const
	{
		float	ParameterValue = Base;

		if (NoiseScale > DELTA)
		{
			for (uint32 Octave = 0; Octave < 4; Octave++)
			{
				float	OctaveShift = static_cast<float>(1 << Octave);
				float	OctaveAmplitude = NoiseAmount / OctaveShift;

				// Attempt to avoid calculating noise if the test value is outside of the noise amplitude.

				if (TestValue > ParameterValue + OctaveAmplitude)
					return 1;
				else if (TestValue < ParameterValue - OctaveAmplitude)
					return 0;
				else
				{
					float	OctaveScale = OctaveShift / NoiseScale;
					ParameterValue += PerlinNoise2D(X * OctaveScale, Y * OctaveScale) * OctaveAmplitude;
				}
			}
		}

		return TestValue >= ParameterValue;
	}

	// TestLess
	bool TestLess(int32 X, int32 Y, float TestValue) const { return !TestGreater(X, Y, TestValue); }

private:
	static const int32 Permutations[256];

	bool operator==(const FNoiseParameter& SrcNoise)
	{
		if ((Base == SrcNoise.Base) &&
			(NoiseScale == SrcNoise.NoiseScale) &&
			(NoiseAmount == SrcNoise.NoiseAmount))
		{
			return true;
		}

		return false;
	}

	void operator=(const FNoiseParameter& SrcNoise)
	{
		Base = SrcNoise.Base;
		NoiseScale = SrcNoise.NoiseScale;
		NoiseAmount = SrcNoise.NoiseAmount;
	}


	float Fade(float T) const
	{
		return T * T * T * (T * (T * 6 - 15) + 10);
	}


	float Grad(int32 Hash, float X, float Y) const
	{
		int32		H = Hash & 15;
		float	U = H < 8 || H == 12 || H == 13 ? X : Y,
			V = H < 4 || H == 12 || H == 13 ? Y : 0;
		return ((H & 1) == 0 ? U : -U) + ((H & 2) == 0 ? V : -V);
	}

	float PerlinNoise2D(float X, float Y) const
	{
		int32		TruncX = FMath::TruncToInt(X),
			TruncY = FMath::TruncToInt(Y),
			IntX = TruncX & 255,
			IntY = TruncY & 255;
		float	FracX = X - TruncX,
			FracY = Y - TruncY;

		float	U = Fade(FracX),
			V = Fade(FracY);

		int32	A = Permutations[IntX] + IntY,
			AA = Permutations[A & 255],
			AB = Permutations[(A + 1) & 255],
			B = Permutations[(IntX + 1) & 255] + IntY,
			BA = Permutations[B & 255],
			BB = Permutations[(B + 1) & 255];

		return	FMath::Lerp(FMath::Lerp(Grad(Permutations[AA], FracX, FracY),
			Grad(Permutations[BA], FracX - 1, FracY), U),
			FMath::Lerp(Grad(Permutations[AB], FracX, FracY - 1),
			Grad(Permutations[BB], FracX - 1, FracY - 1), U), V);
	}
};



#if WITH_KISSFFT
#include "tools/kiss_fftnd.h"
#endif

template<typename DataType>
inline void LowPassFilter(int32 X1, int32 Y1, int32 X2, int32 Y2, FLandscapeBrushData& BrushInfo, TArray<DataType>& Data, const float DetailScale, const float ApplyRatio = 1.0f)
{
#if WITH_KISSFFT
	// Low-pass filter
	int32 FFTWidth = X2 - X1 - 1;
	int32 FFTHeight = Y2 - Y1 - 1;

	if (FFTWidth <= 1 && FFTHeight <= 1)
	{
		// nothing to do
		return;
	}

	const int32 NDims = 2;
	const int32 Dims[NDims] = { FFTHeight, FFTWidth };
	kiss_fftnd_cfg stf = kiss_fftnd_alloc(Dims, NDims, 0, NULL, NULL),
		sti = kiss_fftnd_alloc(Dims, NDims, 1, NULL, NULL);

	kiss_fft_cpx *buf = (kiss_fft_cpx *)KISS_FFT_MALLOC(sizeof(kiss_fft_cpx) * Dims[0] * Dims[1]);
	kiss_fft_cpx *out = (kiss_fft_cpx *)KISS_FFT_MALLOC(sizeof(kiss_fft_cpx) * Dims[0] * Dims[1]);

	for (int32 Y = Y1 + 1; Y <= Y2 - 1; Y++)
	{
		const typename TArray<DataType>::ElementType* DataScanline = Data.GetData() + (Y - Y1) * (X2 - X1 + 1) + (0 - X1);
		kiss_fft_cpx* BufScanline = buf + (Y - (Y1 + 1)) * Dims[1] + (0 - (X1 + 1));

		for (int32 X = X1 + 1; X <= X2 - 1; X++)
		{
			BufScanline[X].r = DataScanline[X];
			BufScanline[X].i = 0;
		}
	}

	// Forward FFT
	kiss_fftnd(stf, buf, out);

	int32 CenterPos[2] = { Dims[0] >> 1, Dims[1] >> 1 };
	for (int32 Y = 0; Y < Dims[0]; Y++)
	{
		float DistFromCenter;
		for (int32 X = 0; X < Dims[1]; X++)
		{
			if (Y < CenterPos[0])
			{
				if (X < CenterPos[1])
				{
					// 1
					DistFromCenter = static_cast<float>(X*X + Y*Y);
				}
				else
				{
					// 2
					DistFromCenter = static_cast<float>((X - Dims[1])*(X - Dims[1]) + Y*Y);
				}
			}
			else
			{
				if (X < CenterPos[1])
				{
					// 3
					DistFromCenter = static_cast<float>(X*X + (Y - Dims[0])*(Y - Dims[0]));
				}
				else
				{
					// 4
					DistFromCenter = static_cast<float>((X - Dims[1])*(X - Dims[1]) + (Y - Dims[0])*(Y - Dims[0]));
				}
			}
			// High frequency removal
			float Ratio = 1.0f - DetailScale;
			float Dist = FMath::Min<float>((Dims[0] * Ratio)*(Dims[0] * Ratio), (Dims[1] * Ratio)*(Dims[1] * Ratio));
			float Filter = 1.0f / (1.0f + DistFromCenter / Dist);
			CA_SUPPRESS(6385);
			out[X + Y*Dims[1]].r *= Filter;
			out[X + Y*Dims[1]].i *= Filter;
		}
	}

	// Inverse FFT
	kiss_fftnd(sti, out, buf);

	const float Scale = static_cast<float>(Dims[0] * Dims[1]);
	const int32 BrushX1 = FMath::Max<int32>(BrushInfo.GetBounds().Min.X, X1 + 1);
	const int32 BrushY1 = FMath::Max<int32>(BrushInfo.GetBounds().Min.Y, Y1 + 1);
	const int32 BrushX2 = FMath::Min<int32>(BrushInfo.GetBounds().Max.X, X2);
	const int32 BrushY2 = FMath::Min<int32>(BrushInfo.GetBounds().Max.Y, Y2);
	for (int32 Y = BrushY1; Y < BrushY2; Y++)
	{
		const float* BrushScanline = BrushInfo.GetDataPtr(FIntPoint(0, Y));
		typename TArray<DataType>::ElementType* DataScanline = Data.GetData() + (Y - Y1) * (X2 - X1 + 1) + (0 - X1);
		const kiss_fft_cpx* BufScanline = buf + (Y - (Y1 + 1)) * Dims[1] + (0 - (X1 + 1));

		for (int32 X = BrushX1; X < BrushX2; X++)
		{
			const float BrushValue = BrushScanline[X];

			if (BrushValue > 0.0f)
			{
				DataScanline[X] = static_cast<DataType>(FMath::Lerp(static_cast<float>(DataScanline[X]), BufScanline[X].r / Scale, BrushValue * ApplyRatio));
			}
		}
	}

	// Free FFT allocation
	KISS_FFT_FREE(stf);
	KISS_FFT_FREE(sti);
	KISS_FFT_FREE(buf);
	KISS_FFT_FREE(out);
#endif
}



//
// TLandscapeEditCache
//
template<class Accessor, typename AccessorType>
struct TLandscapeEditCache
{
public:
	typedef AccessorType DataType;
	typedef Accessor AccessorClass;

	Accessor DataAccess;

	TLandscapeEditCache(const FLandscapeToolTarget& InTarget)
		: DataAccess(InTarget)
		, LandscapeInfo(InTarget.LandscapeInfo)
	{
		check(LandscapeInfo != nullptr);
	}

	// X2/Y2 Coordinates are "inclusive" max values
	// Note that this should maybe be called "ExtendDataCache" because the region here will be combined with the existing cached region, not loaded independently, giving a cached region that is the bounding box of previous and new
	void CacheData(int32 X1, int32 Y1, int32 X2, int32 Y2, bool bCacheOriginalData = false)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(TLandscapeEditCache_CacheData);

		if (!bIsValid)
		{
			if (Accessor::bUseInterp)
			{
				ValidX1 = CachedX1 = X1;
				ValidY1 = CachedY1 = Y1;
				ValidX2 = CachedX2 = X2;
				ValidY2 = CachedY2 = Y2;

				DataAccess.GetData(ValidX1, ValidY1, ValidX2, ValidY2, CachedData);
				if (!ensureMsgf(ValidX1 <= ValidX2 && ValidY1 <= ValidY2, TEXT("Invalid cache area: X(%d-%d), Y(%d-%d) from region X(%d-%d), Y(%d-%d)"), ValidX1, ValidX2, ValidY1, ValidY2, X1, X2, Y1, Y2))
				{
					bIsValid = false;
					return;
				}
			}
			else
			{
				CachedX1 = X1;
				CachedY1 = Y1;
				CachedX2 = X2;
				CachedY2 = Y2;

				DataAccess.GetDataFast(CachedX1, CachedY1, CachedX2, CachedY2, CachedData);
			}

			// Drop a visual log to indicate the area covered by this cache region extension :
			VisualizeLandscapeRegion(CachedX1, CachedY1, CachedX2, CachedY2, FColor::Red, TEXT("Cache Data"));

			if (bCacheOriginalData)
			{
				OriginalData = CachedData;
			}
			bIsValid = true;
		}
		else
		{
			bool bCacheExtended = false;

			// Extend the cache area if needed
			if (X1 < CachedX1)
			{
				if (Accessor::bUseInterp)
				{
					int32 x1 = X1;
					int32 x2 = ValidX1;
					int32 y1 = FMath::Min<int32>(Y1, CachedY1);
					int32 y2 = FMath::Max<int32>(Y2, CachedY2);

					DataAccess.GetData(x1, y1, x2, y2, CachedData);
					ValidX1 = FMath::Min<int32>(x1, ValidX1);
				}
				else
				{
					DataAccess.GetDataFast(X1, CachedY1, CachedX1 - 1, CachedY2, CachedData);
				}

				if (bCacheOriginalData)
				{
					CacheOriginalData(X1, CachedY1, CachedX1 - 1, CachedY2);
				}
				CachedX1 = X1;

				bCacheExtended = true;
			}

			if (X2 > CachedX2)
			{
				if (Accessor::bUseInterp)
				{
					int32 x1 = ValidX2;
					int32 x2 = X2;
					int32 y1 = FMath::Min<int32>(Y1, CachedY1);
					int32 y2 = FMath::Max<int32>(Y2, CachedY2);

					DataAccess.GetData(x1, y1, x2, y2, CachedData);
					ValidX2 = FMath::Max<int32>(x2, ValidX2);
				}
				else
				{
					DataAccess.GetDataFast(CachedX2 + 1, CachedY1, X2, CachedY2, CachedData);
				}
				if (bCacheOriginalData)
				{
					CacheOriginalData(CachedX2 + 1, CachedY1, X2, CachedY2);
				}
				CachedX2 = X2;

				bCacheExtended = true;
			}

			if (Y1 < CachedY1)
			{
				if (Accessor::bUseInterp)
				{
					int32 x1 = CachedX1;
					int32 x2 = CachedX2;
					int32 y1 = Y1;
					int32 y2 = ValidY1;

					DataAccess.GetData(x1, y1, x2, y2, CachedData);
					ValidY1 = FMath::Min<int32>(y1, ValidY1);
				}
				else
				{
					DataAccess.GetDataFast(CachedX1, Y1, CachedX2, CachedY1 - 1, CachedData);
				}
				if (bCacheOriginalData)
				{
					CacheOriginalData(CachedX1, Y1, CachedX2, CachedY1 - 1);
				}
				CachedY1 = Y1;

				bCacheExtended = true;
			}

			if (Y2 > CachedY2)
			{
				if (Accessor::bUseInterp)
				{
					int32 x1 = CachedX1;
					int32 x2 = CachedX2;
					int32 y1 = ValidY2;
					int32 y2 = Y2;

					DataAccess.GetData(x1, y1, x2, y2, CachedData);
					ValidY2 = FMath::Max<int32>(y2, ValidY2);
				}
				else
				{
					DataAccess.GetDataFast(CachedX1, CachedY2 + 1, CachedX2, Y2, CachedData);
				}
				if (bCacheOriginalData)
				{
					CacheOriginalData(CachedX1, CachedY2 + 1, CachedX2, Y2);
				}
				CachedY2 = Y2;

				bCacheExtended = true;
			}

			if (bCacheExtended)
			{
				// Drop a visual log to indicate the area covered by this cache region extension :
				VisualizeLandscapeRegion(CachedX1, CachedY1, CachedX2, CachedY2, FColor::Red, TEXT("Cache Data"));
			}
		}
	}

	AccessorType* GetValueRef(int32 LandscapeX, int32 LandscapeY)
	{
		return CachedData.Find(FIntPoint(LandscapeX, LandscapeY));
	}

	float GetValue(float LandscapeX, float LandscapeY)
	{
		int32 X = FMath::FloorToInt(LandscapeX);
		int32 Y = FMath::FloorToInt(LandscapeY);
		AccessorType* P00 = CachedData.Find(FIntPoint(X, Y));
		AccessorType* P10 = CachedData.Find(FIntPoint(X + 1, Y));
		AccessorType* P01 = CachedData.Find(FIntPoint(X, Y + 1));
		AccessorType* P11 = CachedData.Find(FIntPoint(X + 1, Y + 1));

		// Search for nearest value if missing data
		float V00 = P00 ? *P00 : (P10 ? *P10 : (P01 ? *P01 : (P11 ? *P11 : 0.0f)));
		float V10 = P10 ? *P10 : (P00 ? *P00 : (P11 ? *P11 : (P01 ? *P01 : 0.0f)));
		float V01 = P01 ? *P01 : (P00 ? *P00 : (P11 ? *P11 : (P10 ? *P10 : 0.0f)));
		float V11 = P11 ? *P11 : (P10 ? *P10 : (P01 ? *P01 : (P00 ? *P00 : 0.0f)));

		return FMath::Lerp(
			FMath::Lerp(V00, V10, LandscapeX - X),
			FMath::Lerp(V01, V11, LandscapeX - X),
			LandscapeY - Y);
	}

	FVector GetNormal(int32 X, int32 Y)
	{
		AccessorType* P00 = CachedData.Find(FIntPoint(X, Y));
		AccessorType* P10 = CachedData.Find(FIntPoint(X + 1, Y));
		AccessorType* P01 = CachedData.Find(FIntPoint(X, Y + 1));
		AccessorType* P11 = CachedData.Find(FIntPoint(X + 1, Y + 1));

		// Search for nearest value if missing data
		float V00 = P00 ? *P00 : (P10 ? *P10 : (P01 ? *P01 : (P11 ? *P11 : 0.0f)));
		float V10 = P10 ? *P10 : (P00 ? *P00 : (P11 ? *P11 : (P01 ? *P01 : 0.0f)));
		float V01 = P01 ? *P01 : (P00 ? *P00 : (P11 ? *P11 : (P10 ? *P10 : 0.0f)));
		float V11 = P11 ? *P11 : (P10 ? *P10 : (P01 ? *P01 : (P00 ? *P00 : 0.0f)));

		FVector Vert00 = FVector(0.0f, 0.0f, V00);
		FVector Vert01 = FVector(0.0f, 1.0f, V01);
		FVector Vert10 = FVector(1.0f, 0.0f, V10);
		FVector Vert11 = FVector(1.0f, 1.0f, V11);

		FVector FaceNormal1 = ((Vert00 - Vert10) ^ (Vert10 - Vert11)).GetSafeNormal();
		FVector FaceNormal2 = ((Vert11 - Vert01) ^ (Vert01 - Vert00)).GetSafeNormal();
		return (FaceNormal1 + FaceNormal2).GetSafeNormal();
	}

	void SetValue(int32 LandscapeX, int32 LandscapeY, AccessorType Value)
	{
		CachedData.Add(FIntPoint(LandscapeX, LandscapeY), Forward<AccessorType>(Value));
	}

	bool IsZeroValue(const FVector& Value)
	{
		return (FMath::IsNearlyZero(Value.X) && FMath::IsNearlyZero(Value.Y));
	}

	bool IsZeroValue(const FVector2D& Value)
	{
		return (FMath::IsNearlyZero(Value.X) && FMath::IsNearlyZero(Value.Y));
	}

	bool IsZeroValue(const uint16& Value)
	{
		return Value == 0;
	}

	bool IsZeroValue(const uint8& Value)
	{
		return Value == 0;
	}

	bool HasCachedData(int32 X1, int32 Y1, int32 X2, int32 Y2) const
	{
		return (bIsValid && X1 >= CachedX1 && Y1 >= CachedY1 && X2 <= CachedX2 && Y2 <= CachedY2);
	}
	
	using FPrepareRegionForCachingFunction = TFunction<FIntRect(const FIntRect& NewCacheBounds)>;
	bool GetDataAndCache(int32 X1, int32 Y1, int32 X2, int32 Y2, TArray<AccessorType>& OutData, FPrepareRegionForCachingFunction PrepareRegionForCaching)
	{
		// Drop a visual log to indicate the area requested by this data access :
		VisualizeLandscapeRegion(X1, Y1, X2, Y2, FColor::Blue, TEXT("CacheDataRequest"));

		if (!HasCachedData(X1, Y1, X2, Y2))
		{
			// The cache needs to be expanded, compute the new bounds : 
			// The bounds we calculate here need to be what would be the result of calling CacheData with this region, meaning that they should include the previous bounds. This will let us pass the correct region of interest to PrepareRegionForCaching
			FIntRect NewCacheBounds(bIsValid ? FMath::Min(X1, CachedX1) : X1, bIsValid ? FMath::Min(Y1, CachedY1) : Y1, bIsValid ? FMath::Max(X2, CachedX2) : X2, bIsValid ? FMath::Max(Y2, CachedY2) : Y2);

			// The caller might request a cache region that is actually larger than the data they want to sample for this particular read (e.g. to avoid re-caching when doing expensive samples like layer-collapsing ones) : 
			NewCacheBounds = PrepareRegionForCaching(NewCacheBounds);
			check((NewCacheBounds.Min.X <= X1) && (NewCacheBounds.Min.Y <= Y1) && (NewCacheBounds.Max.X >= X2) && (NewCacheBounds.Max.Y >= Y2));

			CacheData(NewCacheBounds.Min.X, NewCacheBounds.Min.Y, NewCacheBounds.Max.X, NewCacheBounds.Max.Y);
		}
		ensure(HasCachedData(X1, Y1, X2, Y2));
		return GetCachedData(X1, Y1, X2, Y2, OutData);
	}

	// X2/Y2 Coordinates are "inclusive" max values
	bool GetCachedData(int32 X1, int32 Y1, int32 X2, int32 Y2, TArray<AccessorType>& OutData)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(TLandscapeEditCache_GetCachedData);

		const int32 XSize = (1 + X2 - X1);
		const int32 YSize = (1 + Y2 - Y1);
		const int32 NumSamples = XSize * YSize;
		OutData.Empty(NumSamples);
		OutData.AddUninitialized(NumSamples);
		bool bHasNonZero = false;

		for (int32 Y = Y1; Y <= Y2; Y++)
		{
			const int32 YOffset = (Y - Y1) * XSize;
			for (int32 X = X1; X <= X2; X++)
			{
				const int32 XYOffset = YOffset + (X - X1);
				AccessorType* Ptr = GetValueRef(X, Y);
				if (Ptr)
				{
					OutData[XYOffset] = *Ptr;
					if (!IsZeroValue(*Ptr))
					{
						bHasNonZero = true;
					}
				}
				else
				{
					OutData[XYOffset] = (AccessorType)0;
				}
			}
		}

		return bHasNonZero;
	}

	// X2/Y2 Coordinates are "inclusive" max values
	void SetCachedData(int32 X1, int32 Y1, int32 X2, int32 Y2, TArray<AccessorType>& Data, ELandscapeLayerPaintingRestriction PaintingRestriction = ELandscapeLayerPaintingRestriction::None, bool bUpdateData = true)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(TLandscapeEditCache_SetCachedData);

		checkSlow(Data.Num() == (1 + Y2 - Y1) * (1 + X2 - X1));

		// Update cache
		for (int32 Y = Y1; Y <= Y2; Y++)
		{
			for (int32 X = X1; X <= X2; X++)
			{
				SetValue(X, Y, Data[(X - X1) + (Y - Y1)*(1 + X2 - X1)]);
			}
		}

		if (bUpdateData)
		{
			// Update real data
			DataAccess.SetData(X1, Y1, X2, Y2, Data.GetData(), PaintingRestriction);
		}
	}

	// Get the original data before we made any changes with the SetCachedData interface.
	// X2/Y2 Coordinates are "inclusive" max values
	void GetOriginalData(int32 X1, int32 Y1, int32 X2, int32 Y2, TArray<AccessorType>& OutOriginalData)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(TLandscapeEditCache_GetOriginalData);

		int32 NumSamples = (1 + X2 - X1)*(1 + Y2 - Y1);
		OutOriginalData.Empty(NumSamples);
		OutOriginalData.AddUninitialized(NumSamples);

		for (int32 Y = Y1; Y <= Y2; Y++)
		{
			for (int32 X = X1; X <= X2; X++)
			{
				AccessorType* Ptr = OriginalData.Find(FIntPoint(X, Y));
				if (Ptr)
				{
					OutOriginalData[(X - X1) + (Y - Y1)*(1 + X2 - X1)] = *Ptr;
				}
			}
		}
	}

	void Flush()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(TLandscapeEditCache_Flush);
		DataAccess.Flush();
	}

private:
	// X2/Y2 Coordinates are "inclusive" max values
	void CacheOriginalData(int32 X1, int32 Y1, int32 X2, int32 Y2)
	{
		for (int32 Y = Y1; Y <= Y2; Y++)
		{
			for (int32 X = X1; X <= X2; X++)
			{
				FIntPoint Key = FIntPoint(X, Y);
				AccessorType* Ptr = CachedData.Find(Key);
				if (Ptr)
				{
					check(OriginalData.Find(Key) == NULL);
					OriginalData.Add(Key, *Ptr);
				}
			}
		}
	}

	void VisualizeLandscapeRegion(int32 InX1, int32 InY1, int32 InX2, int32 InY2, const FColor& InColor, const FString& InDescription)
	{
		check(LandscapeInfo != nullptr);
		ALandscapeProxy* LandscapeProxy = LandscapeInfo->GetLandscapeProxy();
		check(LandscapeProxy != nullptr);
		const FTransform& LandscapeTransform = LandscapeProxy->GetTransform();
		FVector Min = LandscapeTransform.TransformPosition(FVector(InX1, InY1, 0));
		FVector Max = LandscapeTransform.TransformPosition(FVector(InX2, InY2, 0));
		UE_VLOG_BOX(LandscapeProxy, LogLandscapeTools, Log, FBox(Min, Max), InColor, TEXT("%s"), *InDescription);
	}

	TMap<FIntPoint, AccessorType> CachedData;
	TMap<FIntPoint, AccessorType> OriginalData;
	// Keep the landscape info for visual logging purposes :
	TWeakObjectPtr<ULandscapeInfo> LandscapeInfo;

	bool bIsValid = false;

	int32 CachedX1 = INDEX_NONE;
	int32 CachedY1 = INDEX_NONE;
	int32 CachedX2 = INDEX_NONE;
	int32 CachedY2 = INDEX_NONE;

	// To store valid region....
	int32 ValidX1 = INDEX_NONE;
	int32 ValidX2 = INDEX_NONE;
	int32 ValidY1 = INDEX_NONE;
	int32 ValidY2 = INDEX_NONE;
};

template<bool bInUseInterp>
struct FHeightmapAccessorTool : public FHeightmapAccessor<bInUseInterp>
{
	FHeightmapAccessorTool(const FLandscapeToolTarget& InTarget)
	:	FHeightmapAccessor<bInUseInterp>(InTarget.LandscapeInfo.Get())
	{
	}
};

struct FLandscapeHeightCache : public TLandscapeEditCache<FHeightmapAccessorTool<true>, uint16>
{
	static uint16 ClampValue(int32 Value) { return static_cast<uint16>(FMath::Clamp(Value, 0, LandscapeDataAccess::MaxValue)); }

	FLandscapeHeightCache(const FLandscapeToolTarget& InTarget)
		: TLandscapeEditCache<FHeightmapAccessorTool<true>, uint16>(InTarget)
	{
	}
};

//
// FXYOffsetmapAccessor
//
template<bool bInUseInterp>
struct FXYOffsetmapAccessor
{
	enum { bUseInterp = bInUseInterp };
	FXYOffsetmapAccessor(ULandscapeInfo* InLandscapeInfo)
	{
		LandscapeInfo = InLandscapeInfo;
		LandscapeEdit = new FLandscapeEditDataInterface(InLandscapeInfo);
	}

	FXYOffsetmapAccessor(const FLandscapeToolTarget& InTarget)
		: FXYOffsetmapAccessor(InTarget.LandscapeInfo.Get())
	{
	}

	// accessors
	void GetData(int32& X1, int32& Y1, int32& X2, int32& Y2, TMap<FIntPoint, FVector>& Data)
	{
		LandscapeEdit->GetXYOffsetData(X1, Y1, X2, Y2, Data);

		TMap<FIntPoint, uint16> NewHeights;
		LandscapeEdit->GetHeightData(X1, Y1, X2, Y2, NewHeights);
		for (int32 Y = Y1; Y <= Y2; ++Y)
		{
			for (int32 X = X1; X <= X2; ++X)
			{
				FVector* Value = Data.Find(FIntPoint(X, Y));
				if (Value)
				{
					Value->Z = LandscapeDataAccess::GetLocalHeight(static_cast<uint16>(NewHeights.FindRef(FIntPoint(X, Y))));
				}
			}
		}
	}

	void GetDataFast(int32 X1, int32 Y1, int32 X2, int32 Y2, TMap<FIntPoint, FVector>& Data)
	{
		LandscapeEdit->GetXYOffsetDataFast(X1, Y1, X2, Y2, Data);

		TMap<FIntPoint, uint16> NewHeights;
		LandscapeEdit->GetHeightData(X1, Y1, X2, Y2, NewHeights);
		for (int32 Y = Y1; Y <= Y2; ++Y)
		{
			for (int32 X = X1; X <= X2; ++X)
			{
				FVector* Value = Data.Find(FIntPoint(X, Y));
				if (Value)
				{
					Value->Z = LandscapeDataAccess::GetLocalHeight(static_cast<uint16>(NewHeights.FindRef(FIntPoint(X, Y))));
				}
			}
		}
	}

	void SetData(int32 X1, int32 Y1, int32 X2, int32 Y2, const FVector* Data, ELandscapeLayerPaintingRestriction PaintingRestriction = ELandscapeLayerPaintingRestriction::None)
	{
		TSet<ULandscapeComponent*> Components;
		if (LandscapeInfo && LandscapeEdit->GetComponentsInRegion(X1, Y1, X2, Y2, &Components))
		{
			// Update data
			ChangedComponents.Append(Components);

			// Convert Height to uint16
			TArray<uint16> NewHeights;
			NewHeights.AddZeroed((Y2 - Y1 + 1) * (X2 - X1 + 1));
			for (int32 Y = Y1; Y <= Y2; ++Y)
			{
				for (int32 X = X1; X <= X2; ++X)
				{
					NewHeights[X - X1 + (Y - Y1) * (X2 - X1 + 1)] = LandscapeDataAccess::GetTexHeight(static_cast<float>(Data[(X - X1 + (Y - Y1) * (X2 - X1 + 1))].Z));
				}
			}
						
			// Notify foliage to move any attached instances
			bool bUpdateFoliage = false;
			bool bUpdateNormals = false;
			
			if (!LandscapeEdit->HasLandscapeLayersContent())
			{
				ALandscapeProxy::InvalidateGeneratedComponentData(Components);

				bUpdateNormals = true;
				for (ULandscapeComponent* Component : Components)
				{
					ULandscapeHeightfieldCollisionComponent* CollisionComponent = Component->GetCollisionComponent();
					if (CollisionComponent && AInstancedFoliageActor::HasFoliageAttached(CollisionComponent))
					{
						bUpdateFoliage = true;
						break;
					}
				}
			}

			if (bUpdateFoliage)
			{
				// Calculate landscape local-space bounding box of old data, to look for foliage instances.
				TArray<ULandscapeHeightfieldCollisionComponent*> CollisionComponents;
				CollisionComponents.Empty(Components.Num());
				TArray<FBox> PreUpdateLocalBoxes;
				PreUpdateLocalBoxes.Empty(Components.Num());

				for (ULandscapeComponent* Component : Components)
				{
					CollisionComponents.Add(Component->GetCollisionComponent());
					PreUpdateLocalBoxes.Add(FBox(FVector((float)X1, (float)Y1, Component->CachedLocalBox.Min.Z), FVector((float)X2, (float)Y2, Component->CachedLocalBox.Max.Z)));
				}

				// Update landscape.
				LandscapeEdit->SetXYOffsetData(X1, Y1, X2, Y2, Data, 0); // XY Offset always need to be update before the height update
				LandscapeEdit->SetHeightData(X1, Y1, X2, Y2, NewHeights.GetData(), 0, bUpdateNormals);

				// Snap foliage for each component.
				for (int32 Index = 0; Index < CollisionComponents.Num(); ++Index)
				{
					ULandscapeHeightfieldCollisionComponent* CollisionComponent = CollisionComponents[Index];
					CollisionComponent->SnapFoliageInstances(PreUpdateLocalBoxes[Index].TransformBy(LandscapeInfo->GetLandscapeProxy()->LandscapeActorToWorld().ToMatrixWithScale()).ExpandBy(1.0f));
				}
			}
			else
			{
				// No foliage, just update landscape.
				LandscapeEdit->SetXYOffsetData(X1, Y1, X2, Y2, Data, 0); // XY Offset always need to be update before the height update
				LandscapeEdit->SetHeightData(X1, Y1, X2, Y2, NewHeights.GetData(), 0, bUpdateNormals);
			}
		}
	}

	void Flush()
	{
		LandscapeEdit->Flush();
	}

	virtual ~FXYOffsetmapAccessor()
	{
		delete LandscapeEdit;
		LandscapeEdit = NULL;

		// Update the bounds for the components we edited
		for (TSet<ULandscapeComponent*>::TConstIterator It(ChangedComponents); It; ++It)
		{
			(*It)->UpdateCachedBounds();
			(*It)->UpdateComponentToWorld();
		}
	}

private:
	ULandscapeInfo* LandscapeInfo;
	FLandscapeEditDataInterface* LandscapeEdit;
	TSet<ULandscapeComponent*> ChangedComponents;
};

template<bool bInUseInterp>
struct FLandscapeXYOffsetCache : public TLandscapeEditCache<FXYOffsetmapAccessor<bInUseInterp>, FVector>
{
	FLandscapeXYOffsetCache(const FLandscapeToolTarget& InTarget)
		: TLandscapeEditCache<FXYOffsetmapAccessor<bInUseInterp>, FVector>(InTarget)
	{
	}
};

template<bool bInUseInterp, bool bInUseTotalNormalize>
struct FAlphamapAccessorTool : public FAlphamapAccessor<bInUseInterp, bInUseTotalNormalize>
{
	FAlphamapAccessorTool(ULandscapeInfo* InLandscapeInfo, ULandscapeLayerInfoObject* InLayerInfo)
	:	FAlphamapAccessor<bInUseInterp, bInUseTotalNormalize>(InLandscapeInfo, InLayerInfo)
	{}

	FAlphamapAccessorTool(const FLandscapeToolTarget& InTarget)
	:	FAlphamapAccessor<bInUseInterp, bInUseTotalNormalize>(InTarget.LandscapeInfo.Get(), InTarget.LayerInfo.Get())
	{
	}
};

struct FLandscapeAlphaCache : public TLandscapeEditCache<FAlphamapAccessorTool<true, false>, uint8>
{
	static uint8 ClampValue(int32 Value) { return static_cast<uint8>(FMath::Clamp(Value, 0, 255)); }

	FLandscapeAlphaCache(const FLandscapeToolTarget& InTarget)
		: TLandscapeEditCache<FAlphamapAccessorTool<true, false>, uint8>(InTarget)
	{
	}
};

struct FVisibilityAccessor : public FAlphamapAccessorTool<false, false>
{
	FVisibilityAccessor(const FLandscapeToolTarget& InTarget)
		: FAlphamapAccessorTool<false, false>(InTarget.LandscapeInfo.Get(), ALandscapeProxy::VisibilityLayer)
	{
	}
};

struct FLandscapeVisCache : public TLandscapeEditCache<FAlphamapAccessorTool<false, false>, uint8>
{
	static uint8 ClampValue(int32 Value) { return static_cast<uint8>(FMath::Clamp(Value, 0, 255)); }

	FLandscapeVisCache(const FLandscapeToolTarget& InTarget)
		: TLandscapeEditCache<FAlphamapAccessorTool<false, false>, uint8>(InTarget)
	{
	}
};


template<class ToolTarget>
class FLandscapeLayerDataCache
{
public:
	FLandscapeLayerDataCache(const FLandscapeToolTarget& InTarget, typename ToolTarget::CacheClass& Cache)
		: LandscapeInfo(nullptr)
		, Landscape(nullptr)
		, EditingLayerGuid()
		, EditingLayerIndex(MAX_uint8)
		, bIsInitialized(false)
		, bCombinedLayerOperation(false)
		, bVisibilityChanged(false)
		, bTargetIsHeightmap(InTarget.TargetType == ELandscapeToolTargetType::Heightmap)
		, CacheUpToEditingLayer(Cache)
		, CacheBottomLayers(InTarget)
	{
	}

	void SetCacheEditingLayer(const FGuid& InEditLayerGUID)
	{
		CacheUpToEditingLayer.DataAccess.SetEditLayer(InEditLayerGUID);
		CacheBottomLayers.DataAccess.SetEditLayer(InEditLayerGUID);
		EditingLayerGuid = InEditLayerGUID;
	}

	void Initialize(ULandscapeInfo* InLandscapeInfo, bool InCombinedLayerOperation)
	{
		check(EditingLayerGuid.IsSet());	// you must call SetCacheEditingLayer before Initialize
		if (!bIsInitialized)
		{
			LandscapeInfo = InLandscapeInfo;
			Landscape = LandscapeInfo ? LandscapeInfo->LandscapeActor.Get() : nullptr;
			bCombinedLayerOperation = Landscape && Landscape->HasLayersContent() && InCombinedLayerOperation && bTargetIsHeightmap;
			if (bCombinedLayerOperation)
			{
				for (uint8 i = 0; i < Landscape->GetLayerCount(); ++i)
				{
					FLandscapeLayer* CurrentLayer = Landscape->GetLayer(i);
					BackupLayerVisibility.Add(CurrentLayer->bVisible);
					if (CurrentLayer->Guid == EditingLayerGuid.GetValue())
					{
						EditingLayerIndex = i;
					}
				}
				check(EditingLayerIndex < Landscape->GetLayerCount());
			}
			bIsInitialized = true;
		}
	}

	// read values in the specified rectangle into the array
	void Read(int32 X1, int32 Y1, int32 X2, int32 Y2, TArray<typename ToolTarget::CacheClass::DataType>& Data)
	{
		check(bIsInitialized);
		if (bCombinedLayerOperation)
		{
			TArray<bool> NewLayerVisibility;
			for (int i = 0; i < Landscape->GetLayerCount(); ++i)
			{
				FLandscapeLayer* CurrentLayer = Landscape->GetLayer(i);
				NewLayerVisibility.Add((i > EditingLayerIndex) ? false : CurrentLayer->bVisible);
			}

			auto OnCacheUpdating = [&](const FIntRect& NewCacheBounds) -> FIntRect
			{
				// This function is triggered when the cache needs expanding. We'll ask for a larger area so that we don't need to re-cache (a GPU-synchronizing operation) every time
				//  we need an additional row/column. Aligning the cache region on components borders is appropriate since this is what gets rendered. This avoids
				//  re-caching when we've already sampled a component: 
				FIntRect DesiredCacheBounds;
				DesiredCacheBounds.Min.X = (FMath::FloorToInt((float)NewCacheBounds.Min.X / LandscapeInfo->ComponentSizeQuads)) * LandscapeInfo->ComponentSizeQuads;
				DesiredCacheBounds.Min.Y = (FMath::FloorToInt((float)NewCacheBounds.Min.Y / LandscapeInfo->ComponentSizeQuads)) * LandscapeInfo->ComponentSizeQuads;
				DesiredCacheBounds.Max.X = (FMath::CeilToInt((float)NewCacheBounds.Max.X / LandscapeInfo->ComponentSizeQuads)) * LandscapeInfo->ComponentSizeQuads;
				DesiredCacheBounds.Max.Y = (FMath::CeilToInt((float)NewCacheBounds.Max.Y / LandscapeInfo->ComponentSizeQuads)) * LandscapeInfo->ComponentSizeQuads;

				TSet<ULandscapeComponent*> AffectedComponents;
				LandscapeInfo->GetComponentsInRegion(DesiredCacheBounds.Min.X, DesiredCacheBounds.Min.Y, DesiredCacheBounds.Max.X, DesiredCacheBounds.Max.Y, AffectedComponents);
				SynchronousUpdateComponentVisibilityForHeight(AffectedComponents, NewLayerVisibility);

				if (bVisibilityChanged)
				{
					VisibilityChangedComponents.Append(AffectedComponents);
				}

				return DesiredCacheBounds;
			};

			// temporarily switch to working on the final runtime data, so we can gather the combined layer data into the caches
			FGuid PreviousLayerGUID = EditingLayerGuid.GetValue();
			SetCacheEditingLayer(FGuid());

			CacheUpToEditingLayer.GetDataAndCache(X1, Y1, X2, Y2, Data, OnCacheUpdating);
			// Release Texture Mips that will be Locked by the next SynchronousUpdateComponentVisibilityForHeight
			CacheUpToEditingLayer.DataAccess.Flush();

			// Now turn off visibility on the current layer in order to have the data of all bottom layers except the current one
			NewLayerVisibility[EditingLayerIndex] = false;
			CacheBottomLayers.GetDataAndCache(X1, Y1, X2, Y2, BottomLayersData, OnCacheUpdating);
			// Do the same here for consistency
			CacheBottomLayers.DataAccess.Flush();
			
			SetCacheEditingLayer(PreviousLayerGUID);
			check(PreviousLayerGUID == CacheUpToEditingLayer.DataAccess.GetEditLayer());
		}
		else
		{
			CacheUpToEditingLayer.CacheData(X1, Y1, X2, Y2);
			CacheUpToEditingLayer.GetCachedData(X1, Y1, X2, Y2, Data);
		}
	}

	void Write(int32 X1, int32 Y1, int32 X2, int32 Y2, TArray<typename ToolTarget::CacheClass::DataType>& Data, ELandscapeLayerPaintingRestriction PaintingRestriction = ELandscapeLayerPaintingRestriction::None)
	{
		check(bIsInitialized);
		if (bCombinedLayerOperation)
		{
			const float Alpha = Landscape->GetLayerAlpha(EditingLayerIndex, bTargetIsHeightmap);
			const float InverseAlpha = (Alpha != 0.f) ? 1.f / Alpha : 1.f;
			TArray<typename ToolTarget::CacheClass::DataType> DataContribution;
			DataContribution.Empty(Data.Num());
			DataContribution.AddUninitialized(Data.Num());
			check(Data.Num() == BottomLayersData.Num());
			for (int i = 0; i < Data.Num(); ++i)
			{
				float Contribution = (LandscapeDataAccess::GetLocalHeight(Data[i]) - LandscapeDataAccess::GetLocalHeight(BottomLayersData[i])) * InverseAlpha;
				DataContribution[i] = static_cast<typename ToolTarget::CacheClass::DataType>(LandscapeDataAccess::GetTexHeight(Contribution));
			}

			FGuid CacheAccessorLayerGuid = CacheUpToEditingLayer.DataAccess.GetEditLayer();
 			checkf(EditingLayerGuid.GetValue() == CacheAccessorLayerGuid, TEXT("Editing Layer has changed between Initialize and Write. Was: %s (%s). Is now: %s (%s)"),
 				Landscape->GetLayer(*EditingLayerGuid) ? *(Landscape->GetLayer(*EditingLayerGuid)->Name.ToString()) : TEXT("<unknown>"), *EditingLayerGuid->ToString(),
 				Landscape->GetLayer(CacheAccessorLayerGuid) ? *(Landscape->GetLayer(CacheAccessorLayerGuid)->Name.ToString()) : TEXT("<unknown>"), *CacheAccessorLayerGuid.ToString());

			// Restore layers visibility
			SetLayersVisibility(BackupLayerVisibility);
			// Only store data in cache
			CacheUpToEditingLayer.SetCachedData(X1, Y1, X2, Y2, Data, PaintingRestriction, false);
			// Effectively write the contribution
			CacheUpToEditingLayer.DataAccess.SetData(X1, Y1, X2, Y2, DataContribution.GetData(), PaintingRestriction);
			CacheUpToEditingLayer.DataAccess.Flush();
			if (bVisibilityChanged)
			{
				const bool bUpdateCollision = true;
				const bool bIntermediateRender = false;
				SynchronousUpdateHeightmapForComponents(VisibilityChangedComponents, bUpdateCollision, bIntermediateRender);
				VisibilityChangedComponents.Empty();
				bVisibilityChanged = false;
			}
		}
		else
		{
			CacheUpToEditingLayer.SetCachedData(X1, Y1, X2, Y2, Data, PaintingRestriction);
			CacheUpToEditingLayer.Flush();
		}
	}

private:

	void SynchronousUpdateHeightmapForComponents(const TSet<ULandscapeComponent*>& InComponents, bool bUpdateCollision, bool bIntermediateRender)
	{
		for (ULandscapeComponent* Component : InComponents)
		{
			const bool bUpdateAll = false; // default value
			Component->RequestHeightmapUpdate(bUpdateAll, bUpdateCollision);
		}
		Landscape->ForceUpdateLayersContent(bIntermediateRender);
	};

	void SetLayersVisibility(const TArray<bool>& InLayerVisibility)
	{
		check(InLayerVisibility.Num() == Landscape->GetLayerCount());
		for (int i = 0; i < InLayerVisibility.Num(); ++i)
		{
			if (FLandscapeLayer* Layer = Landscape->GetLayer(i))
			{
				if (Layer->bVisible != InLayerVisibility[i])
				{
					Layer->bVisible = InLayerVisibility[i];
					bVisibilityChanged = true;
				}
			}
		}
	};

	void SynchronousUpdateComponentVisibilityForHeight(const TSet<ULandscapeComponent*>& InComponents, const TArray<bool>& InLayerVisibility)
	{
		SetLayersVisibility(InLayerVisibility);
        // No need to update collision here as we are only doing a intermediate render to gather heightdata
        const bool bUpdateCollision = false;
		const bool bIntermediateRender = true;
		SynchronousUpdateHeightmapForComponents(InComponents, bUpdateCollision, bIntermediateRender);
	};

	ULandscapeInfo* LandscapeInfo;
	ALandscape* Landscape;
	TOptional<FGuid> EditingLayerGuid;
	uint8 EditingLayerIndex;
	TArray<bool> BackupLayerVisibility;
	TArray<typename ToolTarget::CacheClass::DataType> BottomLayersData;
	bool bIsInitialized;
	bool bCombinedLayerOperation;
	bool bVisibilityChanged;
	bool bTargetIsHeightmap;
	TSet<ULandscapeComponent*> VisibilityChangedComponents;

	typename ToolTarget::CacheClass& CacheUpToEditingLayer;
	typename ToolTarget::CacheClass CacheBottomLayers;
};

//
// FFullWeightmapAccessor
//
template<bool bInUseInterp>
struct FFullWeightmapAccessor
{
	enum { bUseInterp = bInUseInterp };
	FFullWeightmapAccessor(ULandscapeInfo* InLandscapeInfo)
		: LandscapeInfo(InLandscapeInfo)
		, LandscapeEdit(InLandscapeInfo)
	{
	}

	FFullWeightmapAccessor(const FLandscapeToolTarget& InTarget)
		: FFullWeightmapAccessor(InTarget.LandscapeInfo.Get())
	{
	}

	~FFullWeightmapAccessor()
	{
		if (!LandscapeEdit.HasLandscapeLayersContent())
		{
			// Recreate collision for modified components to update the physical materials
			for (ULandscapeComponent* Component : ModifiedComponents)
			{
				ULandscapeHeightfieldCollisionComponent* CollisionComponent = Component->GetCollisionComponent();
				if (CollisionComponent)
				{
					CollisionComponent->RecreateCollision();

					// We need to trigger navigation mesh build, in case user have painted holes on a landscape
					if (LandscapeInfo->GetLayerInfoIndex(ALandscapeProxy::VisibilityLayer) != INDEX_NONE)
					{
						FNavigationSystem::UpdateComponentData(*CollisionComponent);
					}
				}
			}
		}
	}

	void SetEditLayer(const FGuid& InEditLayerGUID)
	{
		LandscapeEdit.SetEditLayer(InEditLayerGUID);
	}

	FGuid GetEditLayer() const
	{
		return LandscapeEdit.GetEditLayer();
	}

	void GetData(int32& X1, int32& Y1, int32& X2, int32& Y2, TMap<FIntPoint, TArray<uint8>>& Data)
	{
		// Do not Support for interpolation....
		check(false && TEXT("Do not support interpolation for FullWeightmapAccessor for now"));
	}

	void GetDataFast(int32 X1, int32 Y1, int32 X2, int32 Y2, TMap<FIntPoint, TArray<uint8>>& Data)
	{
		DirtyLayerInfos.Empty();
		LandscapeEdit.GetWeightDataFast(NULL, X1, Y1, X2, Y2, Data);
	}

	void SetData(int32 X1, int32 Y1, int32 X2, int32 Y2, const uint8* Data, ELandscapeLayerPaintingRestriction PaintingRestriction)
	{
		TSet<ULandscapeComponent*> Components;
		if (LandscapeEdit.GetComponentsInRegion(X1, Y1, X2, Y2, &Components))
		{
			if (!LandscapeEdit.HasLandscapeLayersContent())
			{
				ALandscapeProxy::InvalidateGeneratedComponentData(Components);
				ModifiedComponents.Append(Components);
			}
			LandscapeEdit.SetAlphaData(DirtyLayerInfos, X1, Y1, X2, Y2, Data, 0, PaintingRestriction);
		}
		DirtyLayerInfos.Empty();
	}

	void Flush()
	{
		LandscapeEdit.Flush();
	}

	TSet<ULandscapeLayerInfoObject*> DirtyLayerInfos;

private:
	ULandscapeInfo* LandscapeInfo;
	FLandscapeEditDataInterface LandscapeEdit;
	TSet<ULandscapeComponent*> ModifiedComponents;
};

struct FLandscapeFullWeightCache : public TLandscapeEditCache<FFullWeightmapAccessor<false>, TArray<uint8>>
{
	FLandscapeFullWeightCache(const FLandscapeToolTarget& InTarget)
		: TLandscapeEditCache<FFullWeightmapAccessor<false>, TArray<uint8>>(InTarget)
	{
	}

	// Only for all weight case... the accessor type should be TArray<uint8>
	void GetCachedData(int32 X1, int32 Y1, int32 X2, int32 Y2, TArray<uint8>& OutData, int32 ArraySize)
	{
		if (ArraySize == 0)
		{
			OutData.Empty();
			return;
		}

		const int32 XSize = (1 + X2 - X1);
		const int32 YSize = (1 + Y2 - Y1);
		const int32 Stride = XSize * ArraySize;
		int32 NumSamples = XSize * YSize * ArraySize;
		OutData.Empty(NumSamples);
		OutData.AddUninitialized(NumSamples);

		for (int32 Y = Y1; Y <= Y2; Y++)
		{
			const int32 YOffset = (Y - Y1) * Stride;
			for (int32 X = X1; X <= X2; X++)
			{
				const int32 XYOffset = YOffset + (X - X1) * ArraySize;
				TArray<uint8>* Ptr = GetValueRef(X, Y);
				if (Ptr)
				{
					for (int32 Z = 0; Z < ArraySize; Z++)
					{
						OutData[XYOffset + Z] = (*Ptr)[Z];
					}
				}
				else
				{
					FMemory::Memzero((void*)&OutData[XYOffset], (SIZE_T)ArraySize);
				}
			}
		}
	}

	// Only for all weight case... the accessor type should be TArray<uint8>
	void SetCachedData(int32 X1, int32 Y1, int32 X2, int32 Y2, TArray<uint8>& Data, int32 ArraySize, ELandscapeLayerPaintingRestriction PaintingRestriction)
	{
		// Update cache
		for (int32 Y = Y1; Y <= Y2; Y++)
		{
			for (int32 X = X1; X <= X2; X++)
			{
				TArray<uint8> Value;
				Value.Empty(ArraySize);
				Value.AddUninitialized(ArraySize);
				for (int32 Z = 0; Z < ArraySize; Z++)
				{
					Value[Z] = Data[((X - X1) + (Y - Y1)*(1 + X2 - X1)) * ArraySize + Z];
				}
				SetValue(X, Y, MoveTemp(Value));
			}
		}

		// Update real data
		DataAccess.SetData(X1, Y1, X2, Y2, Data.GetData(), PaintingRestriction);
	}

	void AddDirtyLayer(ULandscapeLayerInfoObject* LayerInfo)
	{
		DataAccess.DirtyLayerInfos.Add(LayerInfo);
	}
};

// 
// FDatamapAccessor
//
template<bool bInUseInterp>
struct FDatamapAccessor
{
	enum { bUseInterp = bInUseInterp };
	FDatamapAccessor(ULandscapeInfo* InLandscapeInfo)
		: LandscapeEdit(InLandscapeInfo)
	{
	}

	FDatamapAccessor(const FLandscapeToolTarget& InTarget)
		: FDatamapAccessor(InTarget.LandscapeInfo.Get())
	{
	}

	void GetData(int32& X1, int32& Y1, int32& X2, int32& Y2, TMap<FIntPoint, uint8>& Data)
	{
		LandscapeEdit.GetSelectData(X1, Y1, X2, Y2, Data);
	}

	void GetDataFast(const int32 X1, const int32 Y1, const int32 X2, const int32 Y2, TMap<FIntPoint, uint8>& Data)
	{
		LandscapeEdit.GetSelectData(X1, Y1, X2, Y2, Data);
	}

	void SetData(int32 X1, int32 Y1, int32 X2, int32 Y2, const uint8* Data, ELandscapeLayerPaintingRestriction PaintingRestriction = ELandscapeLayerPaintingRestriction::None)
	{
		if (LandscapeEdit.GetComponentsInRegion(X1, Y1, X2, Y2))
		{
			LandscapeEdit.SetSelectData(X1, Y1, X2, Y2, Data, 0);
		}
	}

	void Flush()
	{
		LandscapeEdit.Flush();
	}

private:
	FLandscapeEditDataInterface LandscapeEdit;
};

struct FLandscapeDataCache : public TLandscapeEditCache<FDatamapAccessor<false>, uint8>
{
	static uint8 ClampValue(int32 Value) { return static_cast<uint8>(FMath::Clamp(Value, 0, 255)); }

	FLandscapeDataCache(const FLandscapeToolTarget& InTarget)
		: TLandscapeEditCache<FDatamapAccessor<false>, uint8>(InTarget)
	{
	}
};


//
// Tool targets
//
struct FHeightmapToolTarget
{
	typedef FLandscapeHeightCache CacheClass;
	static const ELandscapeToolTargetType TargetType = ELandscapeToolTargetType::Heightmap;

	static float StrengthMultiplier(ULandscapeInfo* LandscapeInfo, float BrushRadius)
	{
		if (LandscapeInfo)
		{
			// Adjust strength based on brush size and drawscale, so strength 1 = one hemisphere
			return static_cast<float>(BrushRadius * LANDSCAPE_INV_ZSCALE / LandscapeInfo->DrawScale.Z);
		}
		return 5.0f * LANDSCAPE_INV_ZSCALE;
	}

	static FMatrix ToWorldMatrix(ULandscapeInfo* LandscapeInfo)
	{
		FMatrix Result = FTranslationMatrix(FVector(0, 0, -LandscapeDataAccess::MidValue));
		Result *= FScaleMatrix(FVector(1.0f, 1.0f, LANDSCAPE_ZSCALE) * LandscapeInfo->DrawScale);
		return Result;
	}

	static FMatrix FromWorldMatrix(ULandscapeInfo* LandscapeInfo)
	{
		FMatrix Result = FScaleMatrix(FVector(1.0f, 1.0f, LANDSCAPE_INV_ZSCALE) / (LandscapeInfo->DrawScale));
		Result *= FTranslationMatrix(FVector(0, 0, LandscapeDataAccess::MidValue));
		return Result;
	}
};


struct FWeightmapToolTarget
{
	typedef FLandscapeAlphaCache CacheClass;
	static const ELandscapeToolTargetType TargetType = ELandscapeToolTargetType::Weightmap;

	static float StrengthMultiplier(ULandscapeInfo* LandscapeInfo, float BrushRadius)
	{
		return 255.0f;
	}

	static FMatrix ToWorldMatrix(ULandscapeInfo* LandscapeInfo) { return FMatrix::Identity; }
	static FMatrix FromWorldMatrix(ULandscapeInfo* LandscapeInfo) { return FMatrix::Identity; }
};

/**
 * FLandscapeToolStrokeBase - base class for tool strokes (used by FLandscapeToolBase)
 */

class FLandscapeToolStrokeBase : protected FGCObject
{
public:
	// Whether to call Apply() every frame even if the mouse hasn't moved
	enum { UseContinuousApply = false };

	// This is also the expected signature of derived class constructor used by FLandscapeToolBase
	FLandscapeToolStrokeBase(FEdModeLandscape* InEdMode, FEditorViewportClient* InViewportClient, const FLandscapeToolTarget& InTarget)
		: EdMode(InEdMode)
		, Target(InTarget)
		, LandscapeInfo(InTarget.LandscapeInfo.Get())
	{
	}

	virtual void SetEditLayer(const FGuid& EditLayerGUID)
	{
		// if this function is not overridden, then the tool uses the old method of getting the edit layer (using the shared EditingLayer on ALandscape)
		// we should migrate tools to use SetEditLayer() and deprecate the reliance on the shared EditingLayer
		// once all tools use SetEditLayer we can move this to be part of the constructor
	}

	// Signature of Apply() method for derived strokes
	// void Apply(FEditorViewportClient* ViewportClient, FLandscapeBrush* Brush, const ULandscapeEditorObject* UISettings, const TArray<FLandscapeToolMousePosition>& MousePositions);

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override
	{
		Collector.AddReferencedObject(LandscapeInfo);
	}
	virtual FString GetReferencerName() const override
	{
		return TEXT("FLandscapeToolStrokeBase");
	}

protected:
	FEdModeLandscape* EdMode = nullptr;
	const FLandscapeToolTarget& Target;
	TObjectPtr<ULandscapeInfo> LandscapeInfo = nullptr;
};


/**
 * FLandscapeToolBase - base class for painting tools
 *		ToolTarget - the target for the tool (weight or heightmap)
 *		StrokeClass - the class that implements the behavior for a mouse stroke applying the tool.
 */
template<class TStrokeClass>
class FLandscapeToolBase : public FLandscapeTool
{
	using Super = FLandscapeTool;

public:
	FLandscapeToolBase(FEdModeLandscape* InEdMode)
		: LastInteractorPosition(FVector2D::ZeroVector)
		, TimeSinceLastInteractorMove(0.0f)
		, EdMode(InEdMode)
		, bCanToolBeActivated(true)
		, ToolStroke()
	{
	}

	virtual bool ShouldUpdateEditingLayer() const 
	{ 
		return AffectsEditLayers() && EdMode->CanHaveLandscapeLayersContent();
	}

	virtual ELandscapeLayerUpdateMode GetBeginToolContentUpdateFlag() const
	{
		bool bUpdateHeightmap = this->EdMode->CurrentToolTarget.TargetType == ELandscapeToolTargetType::Heightmap; 
		return bUpdateHeightmap ? ELandscapeLayerUpdateMode::Update_Heightmap_Editing : ELandscapeLayerUpdateMode::Update_Weightmap_Editing;
	}

	virtual ELandscapeLayerUpdateMode GetTickToolContentUpdateFlag() const
	{
		return GetBeginToolContentUpdateFlag();
	}

	virtual ELandscapeLayerUpdateMode GetEndToolContentUpdateFlag() const
	{
		bool bUpdateHeightmap = this->EdMode->CurrentToolTarget.TargetType == ELandscapeToolTargetType::Heightmap;
		return bUpdateHeightmap ? ELandscapeLayerUpdateMode::Update_Heightmap_All : ELandscapeLayerUpdateMode::Update_Weightmap_All;
	}

	virtual bool BeginTool(FEditorViewportClient* ViewportClient, const FLandscapeToolTarget& InTarget, const FVector& InHitLocation) override
	{
		TRACE_BOOKMARK(TEXT("BeginTool - %s"), GetToolName());

		ALandscape* Landscape = this->EdMode->GetLandscape();
		if (Landscape)
		{
			if (ShouldUpdateEditingLayer())
			{
				Landscape->RequestLayersContentUpdate(GetBeginToolContentUpdateFlag());
				Landscape->SetEditingLayer(this->EdMode->GetCurrentLayerGuid());	// legacy way to set the edit layer, via Landscape state
			}
			Landscape->SetGrassUpdateEnabled(false);
		}

		if (!ensure(InteractorPositions.Num() == 0))
		{
			InteractorPositions.Empty(1);
		}

		if (ensure(!IsToolActive()))
		{
			ToolStroke.Emplace( EdMode, ViewportClient, InTarget );				// construct the tool stroke class
			ToolStroke->SetEditLayer(this->EdMode->GetCurrentLayerGuid());		// set the edit layer explicitly (if the tool supports this path)
			EdMode->CurrentBrush->BeginStroke(static_cast<float>(InHitLocation.X), static_cast<float>(InHitLocation.Y), this);
		}

		// Save the mouse position  
		LastInteractorPosition = FVector2D(InHitLocation);
		InteractorPositions.Emplace(LastInteractorPosition, ViewportClient ? IsModifierPressed(ViewportClient) : false); // Copy tool sometimes activates without a specific viewport via ctrl+c hotkey
		TimeSinceLastInteractorMove = 0.0f;

		ToolStroke->Apply(ViewportClient, EdMode->CurrentBrush, EdMode->UISettings, InteractorPositions);

		InteractorPositions.Empty(1);
		return true;
	}

	virtual void Tick(FEditorViewportClient* ViewportClient, float DeltaTime) override
	{
		if (IsToolActive())
		{
			if (InteractorPositions.Num() > 0)
			{
				ToolStroke->Apply(ViewportClient, EdMode->CurrentBrush, EdMode->UISettings, InteractorPositions);
				ViewportClient->Invalidate(false, false);
				InteractorPositions.Empty(1);
			}
			else if (TStrokeClass::UseContinuousApply && TimeSinceLastInteractorMove >= 0.25f)
			{
				InteractorPositions.Emplace(LastInteractorPosition, IsModifierPressed(ViewportClient));
				ToolStroke->Apply(ViewportClient, EdMode->CurrentBrush, EdMode->UISettings, InteractorPositions);
				ViewportClient->Invalidate(false, false);
				InteractorPositions.Empty(1);
			}
			TimeSinceLastInteractorMove += DeltaTime;

			if (ShouldUpdateEditingLayer())
			{
				ALandscape* Landscape = this->EdMode->CurrentToolTarget.LandscapeInfo->LandscapeActor.Get();
				if (Landscape != nullptr)
				{
					Landscape->RequestLayersContentUpdate(GetTickToolContentUpdateFlag());
				}
			}
		}
	}

	virtual void EndTool(FEditorViewportClient* ViewportClient) override
	{
		if (IsToolActive() && InteractorPositions.Num())
		{
			ToolStroke->Apply(ViewportClient, EdMode->CurrentBrush, EdMode->UISettings, InteractorPositions);
			InteractorPositions.Empty(1);
		}

		ToolStroke.Reset();		// destruct the tool stroke class
		EdMode->CurrentBrush->EndStroke();
		EdMode->UpdateLayerUsageInformation(&EdMode->CurrentToolTarget.LayerInfo);

		ALandscape* Landscape = this->EdMode->GetLandscape();
		if (Landscape)
		{
			if (ShouldUpdateEditingLayer())
			{
				Landscape->RequestLayersContentUpdate(GetEndToolContentUpdateFlag());
				Landscape->SetEditingLayer();
			}
			Landscape->SetGrassUpdateEnabled(true);
		}

		TRACE_BOOKMARK(TEXT("EndTool - %s"), GetToolName());
	}

	virtual bool MouseMove(FEditorViewportClient* ViewportClient, FViewport* Viewport, int32 x, int32 y) override
	{
		if (ViewportClient != nullptr && Viewport != nullptr)
		{
			FVector HitLocation;
			if (EdMode->LandscapeMouseTrace(ViewportClient, x, y, HitLocation))
			{
				// If we are moving the mouse to adjust the brush size, don't move the brush
				if (EdMode->CurrentBrush && !EdMode->IsAdjustingBrush(ViewportClient))
				{
					// Inform the brush of the current location, to update the cursor
					EdMode->CurrentBrush->MouseMove(static_cast<float>(HitLocation.X), static_cast<float>(HitLocation.Y));
				}

				if (IsToolActive())
				{
					// Save the interactor position
					if (InteractorPositions.Num() == 0 || LastInteractorPosition != FVector2D(HitLocation))
					{
						LastInteractorPosition = FVector2D(HitLocation);
						InteractorPositions.Emplace(LastInteractorPosition, IsModifierPressed(ViewportClient));
					}
					TimeSinceLastInteractorMove = 0.0f;
				}
			}
		}
		else
		{
			const FVector2D NewPosition(x, y);
			if (InteractorPositions.Num() == 0 || LastInteractorPosition != FVector2D(NewPosition))
			{
				LastInteractorPosition = FVector2D(NewPosition);
				InteractorPositions.Emplace(LastInteractorPosition, IsModifierPressed());
			}
			TimeSinceLastInteractorMove = 0.0f;
		}

		return true;
	}

	virtual bool IsToolActive() const override { return ToolStroke.IsSet();  }

	virtual void SetCanToolBeActivated(bool Value) { bCanToolBeActivated = Value; }
	virtual bool CanToolBeActivated() const {	return bCanToolBeActivated; }

protected:
	TArray<FLandscapeToolInteractorPosition> InteractorPositions;
	FVector2D LastInteractorPosition;
	float TimeSinceLastInteractorMove;
	FEdModeLandscape* EdMode;
	bool bCanToolBeActivated;
	TOptional<TStrokeClass> ToolStroke;

	bool IsModifierPressed(const class FEditorViewportClient* ViewportClient = nullptr)
	{
		UE_LOG(LogLandscapeTools, VeryVerbose, TEXT("ViewportClient = %d, IsShiftDown = %d"), (ViewportClient != nullptr), (ViewportClient != nullptr && IsShiftDown(ViewportClient->Viewport)));
		return ViewportClient != nullptr && IsShiftDown(ViewportClient->Viewport);
	}
};
