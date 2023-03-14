// Copyright Epic Games, Inc. All Rights Reserved.

#include "GerstnerWaterWaves.h"
#include "Engine/Engine.h"
#include "GerstnerWaterWaveSubsystem.h"
#include "Misc/LargeWorldRenderPosition.h"
#include "WaterModule.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GerstnerWaterWaves)

// ----------------------------------------------------------------------------------

void FGerstnerWave::Recompute()
{
	const float Gravity = 980.0f;
	const float Dispersion = 2.0f * PI / WaveLength;
	WaveVector = FVector2D(Direction.X, Direction.Y) * Dispersion;
	WaveSpeed = FMath::Sqrt(Dispersion * Gravity);
	WKA = Amplitude * Dispersion;
	Q = Amplitude * (Steepness / WKA);
}

// ----------------------------------------------------------------------------------

UGerstnerWaterWaves::UGerstnerWaterWaves()
{
	// Default generator
	GerstnerWaveGenerator = CreateDefaultSubobject<UGerstnerWaterWaveGeneratorSimple>(TEXT("WaterWaves"), /* bTransient = */false);

	if (!IsTemplate())
	{
		RecomputeWaves(/* bAllowBPScript = */false); // for the default one, we don't want / cannot call a BP event 
	}
}

void UGerstnerWaterWaves::PostDuplicate(EDuplicateMode::Type DuplicateMode)
{
	Super::PostDuplicate(DuplicateMode);

	if (UGerstnerWaterWaveSubsystem* GerstnerWaterWaveSubsystem = GEngine ? GEngine->GetEngineSubsystem<UGerstnerWaterWaveSubsystem>() : nullptr)
	{
		GerstnerWaterWaveSubsystem->RebuildGPUData();
	}
}

#if WITH_EDITOR
void UGerstnerWaterWaves::PostEditUndo()
{
	Super::PostEditUndo();

	RecomputeWaves(/* bAllowBPScript = */true);
}

void UGerstnerWaterWaves::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	RecomputeWaves(/* bAllowBPScript = */true);
}
#endif // WITH_EDITOR

float UGerstnerWaterWaves::GetWaveHeightAtPosition(const FVector& InPosition, float InWaterDepth, float InTime, FVector& OutNormal) const
{
	float WaveHeight = 0.f;

	FVector SummedNormal(ForceInitToZero);

	// Use the offset of the normalized tile as world position to match the shader behavior (see GerstnerWaveFunctions.ush).
	FVector WorldPosition(FLargeWorldRenderPosition(InPosition).GetOffset());

	for (const FGerstnerWave& Params : GetGerstnerWaves())
	{
		float FirstOffset1D;
		FVector FirstNormal;
		FVector FirstOffset = GetWaveOffsetAtPosition(Params, WorldPosition, InTime, FirstNormal, FirstOffset1D);

		// Only non-zero steepness requires a second sample.
		if (Params.Q != 0)
		{
			//Approximate wave height by taking two samples on each side of the current sample position and lerping.
			//Keep one query point fixed since sampling is going to move the points - if on the left half of wavelength, only add a right offset query point and vice-versa.
			//Choose q as the factor to offset by (max horizontal displacement).
			//Lerp between the two sampled heights and normals.
			const float TwoPi = 2 * PI;
			const float WaveTime = Params.WaveSpeed * InTime;
			float Position1D = FVector2D::DotProduct(FVector2D(WorldPosition.X, WorldPosition.Y), Params.WaveVector) - WaveTime;
			float MappedPosition1D = Position1D >= 0.f ? FMath::Fmod(Position1D, TwoPi) : TwoPi - FMath::Abs(FMath::Fmod(Position1D, TwoPi)); //get positive modulos from negative numbers too

			FVector SecondNormal;
			float SecondOffset1D;
			FVector GuessOffset;
			if (MappedPosition1D < PI)
			{
				GuessOffset = Params.Direction * Params.Q;
			}
			else
			{
				GuessOffset = -Params.Direction * Params.Q;
			}
			const FVector GuessPosition = WorldPosition + GuessOffset;
			FVector SecondOffset = GetWaveOffsetAtPosition(Params, GuessPosition, InTime, SecondNormal, SecondOffset1D);
			SecondOffset1D += (MappedPosition1D < PI) ? Params.Q : -Params.Q;
			if (!(MappedPosition1D < PI))
			{
				Swap<FVector>(FirstOffset, SecondOffset);
				Swap<float>(FirstOffset1D, SecondOffset1D);
				Swap<FVector>(FirstNormal, SecondNormal);
			}
			const float LerpDenominator = (SecondOffset1D - FirstOffset1D);
			float LerpVal = (0 - FirstOffset1D) / (LerpDenominator > 0.f ? LerpDenominator : 1.f);
			const float FinalHeight = FMath::Lerp(FirstOffset.Z, SecondOffset.Z, LerpVal);
			const FVector WaveNormal = FMath::Lerp(FirstNormal, SecondNormal, LerpVal);

			SummedNormal += WaveNormal;
			WaveHeight += FinalHeight;
		}
		else
		{
			SummedNormal += FirstNormal;
			WaveHeight += FirstOffset.Z;
		}
	}
	SummedNormal.Z = 1.0f - SummedNormal.Z;
	OutNormal = SummedNormal.GetSafeNormal();

	return WaveHeight;
}

float UGerstnerWaterWaves::GetSimpleWaveHeightAtPosition(const FVector& InPosition, float InWaterDepth, float InTime) const
{
	float WaveHeight = 0.f;
	// Use the offset of the normalized tile as world position to match the shader behavior (see GerstnerWaveFunctions.ush).
	FVector WorldPosition(FLargeWorldRenderPosition(InPosition).GetOffset());

	for (const FGerstnerWave& Wave : GetGerstnerWaves())
	{
		WaveHeight += GetSimpleWaveOffsetAtPosition(Wave, WorldPosition, InTime);
	}

	return WaveHeight;
}

float UGerstnerWaterWaves::GetWaveAttenuationFactor(const FVector& InPosition, float InWaterDepth, float InTargetWaveMaskDepth) const
{
	const float StrengthCoefficient = FMath::Exp(-FMath::Max(InWaterDepth, 0.0f) / (InTargetWaveMaskDepth / 2.0f));
	return FMath::Clamp(1.f - StrengthCoefficient, 0.f, 1.f);
}

FVector UGerstnerWaterWaves::GetWaveOffsetAtPosition(const FGerstnerWave& InWaveParams, const FVector& InPosition, float InTime, FVector& OutNormal, float& OutOffset1D) const
{
	const float WaveTime = InWaveParams.WaveSpeed * InTime;
	const float WavePosition = FVector2D::DotProduct(FVector2D(InPosition.X, InPosition.Y), InWaveParams.WaveVector) - WaveTime;

	float WaveSin = 0, WaveCos = 0;
	FMath::SinCos(&WaveSin, &WaveCos, WavePosition);
	BlendWaveBetweenLWCTiles(InWaveParams, InPosition, InTime, WaveSin, WaveCos);

	FVector Offset;
	OutOffset1D = -InWaveParams.Q * WaveSin;
	Offset.X = OutOffset1D * InWaveParams.Direction.X;
	Offset.Y = OutOffset1D * InWaveParams.Direction.Y;
	Offset.Z = WaveCos * InWaveParams.Amplitude;

	OutNormal = FVector(WaveSin * InWaveParams.WKA * InWaveParams.Direction.X, WaveSin * InWaveParams.WKA * InWaveParams.Direction.Y, /*WaveCos*InWaveParams.WKA*(InWaveParams.Steepness / InWaveParams.WKA)*/0.f); //match the material

	return Offset;
}

float UGerstnerWaterWaves::GetSimpleWaveOffsetAtPosition(const FGerstnerWave& InWaveParams, const FVector& InPosition, float InTime) const
{
	const float WaveTime = InWaveParams.WaveSpeed * InTime;
	const float WavePosition = FVector2D::DotProduct(FVector2D(InPosition.X, InPosition.Y), InWaveParams.WaveVector) - WaveTime;
	float WaveCos = FMath::Cos(WavePosition);
	float WaveSinDummy = 0.0f;
	BlendWaveBetweenLWCTiles(InWaveParams, InPosition, InTime, WaveSinDummy, WaveCos);
	const float HeightOffset = WaveCos * InWaveParams.Amplitude;
	return HeightOffset;
}

void UGerstnerWaterWaves::BlendWaveBetweenLWCTiles(const FGerstnerWave& InWaveParams, const FVector& InPosition, float InTime, float& WaveSin, float& WaveCos) const
{
	const FVector TileBorderDist = FVector(FLargeWorldRenderScalar::GetTileSize() * 0.5) - InPosition.GetAbs();
	const double BlendZoneWidth = 400.0; // Blend over a range of 4 meters on each side of the tile
	if (TileBorderDist.X < BlendZoneWidth || TileBorderDist.Y < BlendZoneWidth)
	{
		const FVector2D BlendWorldPos = FVector2D(TileBorderDist.X, TileBorderDist.Y);
		const double BlendAlpha = FMath::Clamp(BlendWorldPos.X / BlendZoneWidth, 0.0, 1.0) * FMath::Clamp(BlendWorldPos.Y / BlendZoneWidth, 0.0, 1.0);

		const float WaveTime = InWaveParams.WaveSpeed * InTime;
		const float BlendWavePos = FVector2D::DotProduct(BlendWorldPos, InWaveParams.WaveVector) - WaveTime;
		float BlendWaveSin = 0.0f;
		float BlendWaveCos = 0.0f;
		FMath::SinCos(&BlendWaveSin, &BlendWaveCos, BlendWavePos);
		WaveSin = FMath::Lerp(BlendWaveSin, WaveSin, BlendAlpha);
		WaveCos = FMath::Lerp(BlendWaveCos, WaveCos, BlendAlpha);
	}
}

void UGerstnerWaterWaves::RecomputeWaves(bool bAllowBPScript)
{
	GerstnerWaves.Empty();
	MaxWaveHeight = 0.0f;

	// Generate new waves if there is a generator. Make sure that the wave list has been cleared before generating new ones.
	if (GerstnerWaveGenerator)
	{
		if (bAllowBPScript)
		{
			GerstnerWaveGenerator->GenerateGerstnerWaves(GerstnerWaves);
		}
		else
		{
			GerstnerWaveGenerator->GenerateGerstnerWaves_Implementation(GerstnerWaves);
		}

		// Automatically recompute the waves internals after waves have been regenerated :
		for (FGerstnerWave& Params : GerstnerWaves)
		{
			Params.Recompute();
			MaxWaveHeight += Params.Amplitude;
		}
	}

	if (UGerstnerWaterWaveSubsystem* GerstnerWaterWaveSubsystem = GEngine ? GEngine->GetEngineSubsystem<UGerstnerWaterWaveSubsystem>() : nullptr)
	{
		GerstnerWaterWaveSubsystem->RebuildGPUData();
	}
}

// ----------------------------------------------------------------------------------

void UGerstnerWaterWaveGeneratorSimple::GenerateGerstnerWaves_Implementation(TArray<FGerstnerWave>& OutWaves) const
{
	ensure(OutWaves.Num() == 0);

	FRandomStream LocalSeed(Seed);
	//int32 Quality = GetQualityWaveCount(); // Replaced by NumWaves

	for (int i = 0; i < NumWaves; ++i)
	{
		float Alpha = FMath::Clamp(1.f - ((float)i / (float)NumWaves) + LocalSeed.FRandRange(Randomness * (-1.0f / (float)NumWaves), Randomness * (1.0f / (float)NumWaves)), 0.0f, 1.0f);

		FGerstnerWave& Params = OutWaves.AddDefaulted_GetRef();

		Params.Direction = FVector(EForceInit::ForceInitToZero);
		FMath::SinCos(&Params.Direction.Y, &Params.Direction.X, FMath::DegreesToRadians((FVector::FReal)WindAngleDeg));
		if (i > 0)
		{
			Params.Direction = Params.Direction.RotateAngleAxis(LocalSeed.FRandRange(-DirectionAngularSpreadDeg, DirectionAngularSpreadDeg), FVector::UpVector);
		}

		Params.WaveLength = FMath::Lerp(MinWavelength, MaxWavelength, FMath::Pow(Alpha, WavelengthFalloff));
		Params.Amplitude = FMath::Max(FMath::Lerp(MinAmplitude, MaxAmplitude, FMath::Pow(Alpha, AmplitudeFalloff)), 0.0001f);
		Params.Steepness = FMath::Lerp(LargeWaveSteepness, SmallWaveSteepness, FMath::Pow((float)i / NumWaves, SteepnessFalloff));
	}
}

// ----------------------------------------------------------------------------------

void UGerstnerWaterWaveGeneratorSpectrum::GenerateGerstnerWaves_Implementation(TArray<FGerstnerWave>& OutWaves) const
{
	// [todo] kevin.ortegren: implement	
}


