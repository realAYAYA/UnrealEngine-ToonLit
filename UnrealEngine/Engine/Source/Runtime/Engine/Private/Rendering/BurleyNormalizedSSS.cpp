// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
BurleyNormalizedSSS.cpp: Compute the transmition profile for Burley normalized SSS
=============================================================================*/


#include "Rendering/BurleyNormalizedSSS.h"
#include "Math/Vector.h"
#include "HAL/IConsoleManager.h"

static TAutoConsoleVariable<int32> CVarSSProfilesTransmissionUseLegacy(
	TEXT("r.SSProfiles.Transmission.UseLegacy"),
	1,
	TEXT("0. Use more physically correct transmission profile.\n")
	TEXT("1. Use legacy transmission profile (default)."),
	ECVF_RenderThreadSafe
);

// estimated from the sampling interval, 1/TargetBufferSize(1/32) and MaxTransmissionProfileDistance. If any is changed, this parameter should be re-estimated.
const float ProfileRadiusOffset = 0.06;

inline float Burley_ScatteringProfile(float r, float A,float S, float L)
{   //2PIR(r)r
	float D = 1 / S;
	float R = r / L;
	const float Inv8Pi = 1.0 / (8 * UE_PI);
	float NegRbyD = -R / D;
	float RrDotR = A*FMath::Max((exp(NegRbyD) + exp(NegRbyD / 3.0)) / (D*L)*Inv8Pi, 0.0);
	return RrDotR;
}

inline float Burley_TransmissionProfile(float r, float A, float S, float L)
{   
	//integration from t to infty
	return 0.25* A * (exp(-S * r/L) + 3 * exp(-S * r / (3*L)));
}


inline FVector Burley_ScatteringProfile(float RadiusInMm, FLinearColor SurfaceAlbedo, FVector ScalingFactor, FLinearColor DiffuseMeanFreePathInMm)
{  
	return FVector(	Burley_ScatteringProfile(RadiusInMm, SurfaceAlbedo.R, ScalingFactor.X, DiffuseMeanFreePathInMm.R) ,
					Burley_ScatteringProfile(RadiusInMm, SurfaceAlbedo.G, ScalingFactor.Y, DiffuseMeanFreePathInMm.G) ,
					Burley_ScatteringProfile(RadiusInMm, SurfaceAlbedo.B, ScalingFactor.Z, DiffuseMeanFreePathInMm.B));
}

inline FLinearColor Burley_TransmissionProfile(float RadiusInMm, FLinearColor SurfaceAlbedo, FVector ScalingFactor, FLinearColor DiffuseMeanFreePathInMm)
{
	return FLinearColor(Burley_TransmissionProfile(RadiusInMm, SurfaceAlbedo.R, ScalingFactor.X, DiffuseMeanFreePathInMm.R),
						Burley_TransmissionProfile(RadiusInMm, SurfaceAlbedo.G, ScalingFactor.Y, DiffuseMeanFreePathInMm.G),
						Burley_TransmissionProfile(RadiusInMm, SurfaceAlbedo.B, ScalingFactor.Z, DiffuseMeanFreePathInMm.B));
}

//--------------------------------------------------------------------------
//Map burley ColorFallOff to Burley SurfaceAlbedo and diffuse mean free path.
void MapFallOffColor2SurfaceAlbedoAndDiffuseMeanFreePath(float FalloffColor, float& SurfaceAlbedo, float& DiffuseMeanFreePath)
{
	//@TODO, use picewise function to separate Falloffcolor to around (0,0.2) and (0.2, 1) to make it more correct
	//map Falloffcolor to SurfaceAlbedo with 4 polynomial, error < 2e-3.
	float X = FalloffColor;
	float X2 = X * X;
	float X4 = X2 * X2;
#if 0
	// max error happens around 0.1, which is -4.8e-3. The others are less than 2.5e-3.
	SurfaceAlbedo = 5.883 * X4 * X2 - 19.88 * X4 * X + 26.08 * X4 - 16.59 * X2 * X + 5.143 * X2 + 0.2636 * X + 0.01098;
	// max error happens around 0.1, which is -3.8e-3.
	DiffuseMeanFreePath = 4.78 * X4 * X2 - 5.178 * X4 * X + 5.2154 * X4 - 4.424 * X2 * X + 1.636 * X2 + 0.4067 * X + 0.006853;
#else
	// max error happens around 0, which is 1e-4. The others are less than 2e-5.
	SurfaceAlbedo = 0.906 * X + 0.00004;
	// max error happens around 0.95, which is -1e-4.
	DiffuseMeanFreePath = 10.39 * X4 * X -15.18 * X4 + 8.332 * X2 * X -2.039 * X2 + 0.7279 * X - 0.0014;
#endif
}

//-----------------------------------------------------------------
// Functions should be identical on both cpu and gpu
// Method 1: The light directly goes into the volume in a direction perpendicular to the surface.
// Average relative error: 5.5% (reference to MC)
float GetPerpendicularScalingFactor(float SurfaceAlbedo)
{
	// add abs() to match the formula in the original paper. 
	return 1.85 - SurfaceAlbedo + 7 * FMath::Pow(FMath::Abs(SurfaceAlbedo - 0.8), 3);
}

FVector GetPerpendicularScalingFactor(FLinearColor SurfaceAlbedo)
{
	return FVector(GetPerpendicularScalingFactor(SurfaceAlbedo.R),
		GetPerpendicularScalingFactor(SurfaceAlbedo.G),
		GetPerpendicularScalingFactor(SurfaceAlbedo.B)
	);
}

// Method 2: Ideal diffuse transmission at the surface. More appropriate for rough surface.
// Average relative error: 3.9% (reference to MC)
float GetDiffuseSurfaceScalingFactor(float SurfaceAlbedo)
{
	return 1.9 - SurfaceAlbedo + 3.5 * FMath::Pow(SurfaceAlbedo - 0.8, 2);
}

FVector GetDiffuseSurfaceScalingFactor(FLinearColor SurfaceAlbedo)
{
	return FVector(GetDiffuseSurfaceScalingFactor(SurfaceAlbedo.R),
		GetDiffuseSurfaceScalingFactor(SurfaceAlbedo.G),
		GetDiffuseSurfaceScalingFactor(SurfaceAlbedo.B)
	);
}

// Method 3: The spectral of diffuse mean free path on the surface.
// Avergate relative error: 7.7% (reference to MC)
float GetSearchLightDiffuseScalingFactor(float SurfaceAlbedo)
{
	return 3.5 + 100 * FMath::Pow(SurfaceAlbedo - 0.33, 4);
}

FVector GetSearchLightDiffuseScalingFactor(FLinearColor SurfaceAlbedo)
{
	return FVector(GetSearchLightDiffuseScalingFactor(SurfaceAlbedo.R),
		GetSearchLightDiffuseScalingFactor(SurfaceAlbedo.G),
		GetSearchLightDiffuseScalingFactor(SurfaceAlbedo.B)
	);
}

FLinearColor GetMeanFreePathFromDiffuseMeanFreePath(FLinearColor SurfaceAlbedo, FLinearColor DiffuseMeanFreePath)
{
	return DiffuseMeanFreePath * FLinearColor(GetPerpendicularScalingFactor(SurfaceAlbedo) / GetSearchLightDiffuseScalingFactor(SurfaceAlbedo));
}

FLinearColor GetDiffuseMeanFreePathFromMeanFreePath(FLinearColor SurfaceAlbedo, FLinearColor MeanFreePath)
{
	return MeanFreePath * FLinearColor(GetSearchLightDiffuseScalingFactor(SurfaceAlbedo) / GetPerpendicularScalingFactor(SurfaceAlbedo));
}

void ComputeMirroredBSSSKernel(FLinearColor* TargetBuffer, uint32 TargetBufferSize,
	FLinearColor SurfaceAlbedo, FLinearColor DiffuseMeanFreePath, float ScatterRadius)
{
	check(TargetBuffer);
	check(TargetBufferSize > 0);

	uint32 nNonMirroredSamples = TargetBufferSize;
	int32 nTotalSamples = nNonMirroredSamples * 2 - 1;

	FVector ScalingFactor = GetSearchLightDiffuseScalingFactor(SurfaceAlbedo);
	// we could generate Out directly but the original code form SeparableSSS wasn't done like that so we convert it later
	// .a is in mm
	check(nTotalSamples < 64);

	FLinearColor kernel[64];
	{
		const float Range = (nTotalSamples > 20 ? 3.0f : 2.0f);
		// tweak constant
		const float Exponent = 2.0f;

		// Calculate the offsets:
		float step = 2.0f * Range / (nTotalSamples - 1);
		for (int i = 0; i < nTotalSamples; i++)
		{
			float o = -Range + float(i) * step;
			float sign = o < 0.0f ? -1.0f : 1.0f;
			kernel[i].A = Range * sign * FMath::Abs(FMath::Pow(o, Exponent)) / FMath::Pow(Range, Exponent);
		}
		// Center sample should always be zero, but might not be due to potential roundoff error.
		kernel[nTotalSamples / 2].A = 0.0f;

		//Scale the profile sampling radius. This scale enables the sampling between [-3*SpaceScale,+3*SpaceScale] instead of 
		//the default [-3,3] range when fetching kernel parameters.
		const float SpaceScale = ScatterRadius * 10.0f;// from cm to mm

		// Calculate the weights:
		for (int32 i = 0; i < nTotalSamples; i++)
		{
			float w0 = i > 0 ? FMath::Abs(kernel[i].A - kernel[i - 1].A) : 0.0f;
			float w1 = i < nTotalSamples - 1 ? FMath::Abs(kernel[i].A - kernel[i + 1].A) : 0.0f;
			float area = (w0 + w1) / 2.0f;
			FVector t = area * Burley_ScatteringProfile(FMath::Abs(kernel[i].A)*SpaceScale, SurfaceAlbedo, ScalingFactor,DiffuseMeanFreePath);
			kernel[i].R = t.X;
			kernel[i].G = t.Y;
			kernel[i].B = t.Z;
		}

		// We still need to do a small tweak to get the radius to visually match. Multiplying by 2.0 seems to fix it.
		// Match that in GetSubsurfaceProfileKernel in PostProcessSubsurface.usf
		const float StepScale = 2.0f;
		for (int32 i = 0; i < nTotalSamples; i++)
		{
			kernel[i].A *= StepScale;
		}

		// We want the offset 0.0 to come first:
		FLinearColor t = kernel[nTotalSamples / 2];

		for (int i = nTotalSamples / 2; i > 0; i--)
		{
			kernel[i] = kernel[i - 1];
		}
		kernel[0] = t;

		// Normalize the weights in RGB
		{
			FVector sum = FVector(0, 0, 0);

			for (int i = 0; i < nTotalSamples; i++)
			{
				sum.X += kernel[i].R;
				sum.Y += kernel[i].G;
				sum.Z += kernel[i].B;
			}

			for (int i = 0; i < nTotalSamples; i++)
			{
				kernel[i].R /= sum.X;
				kernel[i].G /= sum.Y;
				kernel[i].B /= sum.Z;
			}
		}

		/* we do that in the shader for better quality with half res

		// Tweak them using the desired strength. The first one is:
		//     lerp(1.0, kernel[0].rgb, strength)
		kernel[0].R = FMath::Lerp(1.0f, kernel[0].R, SubsurfaceColor.R);
		kernel[0].G = FMath::Lerp(1.0f, kernel[0].G, SubsurfaceColor.G);
		kernel[0].B = FMath::Lerp(1.0f, kernel[0].B, SubsurfaceColor.B);

		for (int i = 1; i < nTotalSamples; i++)
		{
			kernel[i].R *= SubsurfaceColor.R;
			kernel[i].G *= SubsurfaceColor.G;
			kernel[i].B *= SubsurfaceColor.B;
		}*/
	}

	// generate output (remove negative samples)
	{
		// center sample
		TargetBuffer[0] = kernel[0];

		// all positive samples
		for (uint32 i = 0; i < nNonMirroredSamples - 1; i++)
		{
			TargetBuffer[i + 1] = kernel[nNonMirroredSamples + i];
		}
	}
}



void ComputeTransmissionProfileBurley(FLinearColor* TargetBuffer, uint32 TargetBufferSize, 
									FLinearColor FalloffColor, float ExtinctionScale,
									FLinearColor SurfaceAlbedo, FLinearColor DiffuseMeanFreePathInMm,
									float WorldUnitScale, FLinearColor TransmissionTintColor)
{
	check(TargetBuffer);
	check(TargetBufferSize > 0);

	// Unit scale should be independent to the base unit.
	// Example of scaling
	// ----------------------------------------
	// DistanceCm * UnitScale * CmToMm = Value (mm)
	// ----------------------------------------
	//   1          0.1         10     =   1mm
	//   1          1.0         10     =  10mm
	//   1         10.0         10     = 100mm

	const float SubsurfaceScatteringUnitInCm = 0.1f;
	const float UnitScale = WorldUnitScale / SubsurfaceScatteringUnitInCm;
	float InvUnitScale = 1.0 / UnitScale; // Scaling the unit is equivalent to inverse scaling of the profile.

	if (CVarSSProfilesTransmissionUseLegacy.GetValueOnAnyThread() == 1)
	{
		InvUnitScale *= 0.1f; // Backward compatibility
	}

	static float MaxTransmissionProfileDistance = 5.0f; // See SSSS_MAX_TRANSMISSION_PROFILE_DISTANCE in TransmissionCommon.ush
	static float CmToMm = 10.0f;
	//assuming that the volume albedo is the same to the surface albedo for transmission.
	FVector ScalingFactor = GetSearchLightDiffuseScalingFactor(SurfaceAlbedo);

	const float InvSize = 1.0f / TargetBufferSize;

	for (uint32 i = 0; i < TargetBufferSize; ++i)
	{
		const float DistanceInMm = i * InvSize * (MaxTransmissionProfileDistance * CmToMm) * InvUnitScale;
		const float OffsetInMm = (ProfileRadiusOffset * CmToMm) * InvUnitScale;


		FLinearColor TransmissionProfile = Burley_TransmissionProfile(DistanceInMm + OffsetInMm, SurfaceAlbedo, ScalingFactor, DiffuseMeanFreePathInMm);
		TargetBuffer[i] = TransmissionProfile * TransmissionTintColor; // Apply tint to the profile
		//Use Luminance of scattering as SSSS shadow.
		TargetBuffer[i].A = exp(-DistanceInMm * ExtinctionScale);
	}

	// Do this is because 5mm is not enough cool down the scattering to zero, although which is small number but after tone mapping still noticeable
	// so just Let last pixel be 0 which make sure thickness great than MaxRadius have no scattering
	static bool bMakeLastPixelBlack = true;
	if (bMakeLastPixelBlack)
	{
		TargetBuffer[TargetBufferSize - 1] = FLinearColor::Black;
	}
}
