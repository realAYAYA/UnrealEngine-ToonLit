// Copyright Epic Games, Inc. All Rights Reserved.

#include "SphericalHarmonicCalculator.h"
#include "DSP/Dsp.h"

namespace SHBasis
{
#pragma region FOA
	// zero order
	static float SHBasisFunction_ACN_0(float, float)
	{
		return 1.0f;
	}

	// first order
	static float SHBasisFunction_ACN_1(float Azimuth, float Elevation)
	{
		return FMath::Sin(Azimuth) * FMath::Cos(Elevation);
	}

	static float SHBasisFunction_ACN_2(float, float Elevation)
	{
		return FMath::Sin(Elevation);
	}

	static float SHBasisFunction_ACN_3(float Azimuth, float Elevation)
	{
		return FMath::Cos(Azimuth) * FMath::Cos(Elevation);
	}
#pragma  endregion
#pragma region SECOND_ORDER
	static const float MagicNumber_ACN_4 = FMath::Sqrt(3.0f) / 2.0f;
	static float SHBasisFunction_ACN_4(const float Azimuth, const float Elevation)
	{
		const float CosElevation = FMath::Cos(Elevation);
		return MagicNumber_ACN_4 * FMath::Sin(2.0f * Azimuth) * CosElevation*CosElevation;
	}

	static const float MagicNumber_ACN_5 = FMath::Sqrt(3.0f) / 2.0f;
	static float SHBasisFunction_ACN_5(const float Azimuth, const float Elevation)
	{
		return MagicNumber_ACN_5 * FMath::Sin(Azimuth) * FMath::Sin(2.0f * Elevation);
	}

	static float SHBasisFunction_ACN_6(const float Azimuth, const float Elevation)
	{
		const float SinElevation = FMath::Sin(Elevation);
		return 1.5f * SinElevation*SinElevation - 0.5f;
	}

	static const float MagicNumber_ACN_7 = FMath::Sqrt(3.0f) / 2.0f;
	static float SHBasisFunction_ACN_7(const float Azimuth, const float Elevation)
	{
		return MagicNumber_ACN_7 * FMath::Cos(Azimuth) * FMath::Sin(2.0f * Elevation);
	}

	static const float MagicNumber_ACN_8 = FMath::Sqrt(3.0f) / 2.0f;
	static float SHBasisFunction_ACN_8(const float Azimuth, const float Elevation)
	{
		static const float CosElevation = FMath::Cos(Elevation);
		return MagicNumber_ACN_8 * FMath::Cos(2.07 * Azimuth) * CosElevation*CosElevation;
	}
	#pragma  endregion
#pragma region THIRD_ORDER
	static const float MagicNumber_ACN_9 = FMath::Sqrt(10.0f) / 4.0f;
	static float SHBasisFunction_ACN_9(float Azimuth, float Elevation)
	{
		const float CosElevation = FMath::Cos(Elevation);
		return  MagicNumber_ACN_9 * FMath::Sin(3.0f * Azimuth) * CosElevation*CosElevation*CosElevation;
	}

	static const float MagicNumber_ACN_10 = 0.5f * FMath::Sqrt(15.0f);
	static float SHBasisFunction_ACN_10(float Azimuth, float Elevation)
	{
		static const float CosElevation = FMath::Cos(Elevation);
		return MagicNumber_ACN_10 * FMath::Sin(2.0f * Azimuth) * FMath::Sin(Elevation) * CosElevation*CosElevation;
	}

	static const float MagicNumber_ACN_11 = 0.25f * FMath::Sqrt(6.0f);
	static float SHBasisFunction_ACN_11(float Azimuth, float Elevation)
	{
		static const float SinElevation = FMath::Sin(Elevation);
		return MagicNumber_ACN_11 * FMath::Sin(Azimuth) * FMath::Cos(Elevation) * (5.0f * SinElevation*SinElevation - 1.0f);
	}

	static float SHBasisFunction_ACN_12(float Azimuth, float Elevation)
	{
		const float SinElevation = FMath::Sin(Elevation);
		return FMath::Sin(Elevation) * (2.5f * SinElevation*SinElevation - 1.5f);
	}

	static const float MagicNumber_ACN_13 = 0.25f * FMath::Sqrt(6.0f);
	static float SHBasisFunction_ACN_13(float Azimuth, float Elevation)
	{
		const float SinElevation = FMath::Sin(Elevation);
		return MagicNumber_ACN_13 * FMath::Cos(Azimuth) * FMath::Cos(Elevation)* (5.0f * SinElevation*SinElevation - 1.0f);
	}

	static const float MagicNumber_ACN_14 = 0.5f * FMath::Sqrt(15.0f);
	static float SHBasisFunction_ACN_14(float Azimuth, float Elevation)
	{
		const float CosElevation = FMath::Cos(Elevation);
		return MagicNumber_ACN_14 * FMath::Cos(2.0f * Azimuth) * FMath::Sin(Elevation) * CosElevation*CosElevation;
	}

	static const float MagicNumber_ACN_15 = 0.25f * FMath::Sqrt(10.0f);
	static float SHBasisFunction_ACN_15(float Azimuth, float Elevation)
	{
		static const float CosElevation = FMath::Cos(Elevation);
		return MagicNumber_ACN_15 * FMath::Cos(3.0f * Azimuth) * CosElevation*CosElevation*CosElevation;
	}
#pragma  endregion
#pragma region FORTH_ORDER
	static const float MagicNumber_ACN_16 = FMath::Sqrt(35.0f) / 8.0f;
	static float SHBasisFunction_ACN_16(float Azimuth, float Elevation)
	{
		static const float CosElevation = FMath::Cos(Elevation);
		return MagicNumber_ACN_16 * FMath::Sin(4.0f * Azimuth) * CosElevation*CosElevation*CosElevation*CosElevation;
	}

	static const float MagicNumber_ACN_17 = 0.25f * FMath::Sqrt(70.0f);
	static float SHBasisFunction_ACN_17(float Azimuth, float Elevation)
	{
		const float CosElevation = FMath::Cos(Elevation);
		return MagicNumber_ACN_17 * FMath::Sin(3.0f * Azimuth) * FMath::Sin(Elevation) * CosElevation*CosElevation*CosElevation;
	}

	static const float MagicNumber_ACN_18 = 0.25f * FMath::Sqrt(5.0f);
	static float SHBasisFunction_ACN_18(float Azimuth, float Elevation)
	{
		static const float SinElevation = FMath::Sin(Elevation);
		static const float CosElevation = FMath::Cos(Elevation);
		return MagicNumber_ACN_18 * FMath::Sin(2.0f * Azimuth) * (7.0f * SinElevation*SinElevation - 1.0f) * CosElevation*CosElevation;
	}

	static const float MagicNumber_ACN_19 = FMath::Sqrt(10.0f) / 8.0f;
	static float SHBasisFunction_ACN_19(float Azimuth, float Elevation)
	{
		static const float SinElevation = FMath::Sin(Elevation);
		return MagicNumber_ACN_19 * FMath::Sin(Azimuth) * FMath::Sin(2.0f * Elevation) * (7.0f * SinElevation*SinElevation - 3.0f);
	}

	static float SHBasisFunction_ACN_20(float Azimuth, float Elevation)
	{
		const float SinElevation = FMath::Sin(Elevation);
		const float SquaredSinElevation = SinElevation*SinElevation;
		return 0.625f * SquaredSinElevation*(7.0f * SquaredSinElevation - 3.0f);
	}

	static const float MagicNumber_ACN_21 = FMath::Sqrt(10.0f) / 8.0f;
	static float SHBasisFunction_ACN_21(float Azimuth, float Elevation)
	{
		static const float SinElevation = FMath::Sin(Elevation);
		return MagicNumber_ACN_21 * FMath::Cos(Azimuth) * FMath::Sin(2.0f * Elevation) * (7.0f * SinElevation*SinElevation  - 3.0f);
	}
	
	static const float MagicNumber_ACN_22 = 0.25f * FMath::Sqrt(5.0f);
	static float SHBasisFunction_ACN_22(float Azimuth, float Elevation)
	{
		const float SinElevation = FMath::Sin(Elevation);
		const float CosElevation = FMath::Cos(Elevation);
		return MagicNumber_ACN_22 * FMath::Cos(2.0f * Azimuth) * (7.0f * SinElevation*SinElevation - 1.0f) * CosElevation*CosElevation;
	}

	static const float MagicNumber_ACN_23 = 0.25f * FMath::Sqrt(70.0f);
	static float SHBasisFunction_ACN_23(float Azimuth, float Elevation)
	{
		const float CosElevation = FMath::Cos(Elevation);
		return MagicNumber_ACN_23 * FMath::Cos(3.0f * Azimuth) * FMath::Sin(Elevation) * CosElevation*CosElevation*CosElevation;
	}

	static const float MagicNumber_ACN_24 = FMath::Sqrt(35.0f) / 8.0f;
	static float SHBasisFunction_ACN_24(float Azimuth, float Elevation)
	{
		const float CosElevation = FMath::Cos(Elevation);
		return MagicNumber_ACN_24 * FMath::Cos(4.0f * Azimuth) * CosElevation*CosElevation*CosElevation*CosElevation;
	}
#pragma endregion
#pragma region FIFTH_ORDER

	static const float MagicNumber_ACN_25 = 3.0f * FMath::Sqrt(14.0f) / 16.0f;
	static float SHBasisFunction_ACN_25(const float Azimuth, const float Elevation)
	{
		const float CosElevation = FMath::Cos(Elevation);
		return MagicNumber_ACN_25 * FMath::Sin(5.0f * Azimuth) * CosElevation*CosElevation*CosElevation*CosElevation*CosElevation;
	}

	static const float MagicNumber_ACN_26 = 3.0f * FMath::Sqrt(35.0f) / 8.0f;
	static float SHBasisFunction_ACN_26(const float Azimuth, const float Elevation)
	{
		const float CosElevation = FMath::Cos(Elevation);
		return  MagicNumber_ACN_26 * FMath::Sin(4.0f * Azimuth) * FMath::Sin(Elevation) * CosElevation*CosElevation*CosElevation*CosElevation;
	}

	static const float MagicNumber_ACN_27 = FMath::Sqrt(70.0f) / 32.0f;
	static float SHBasisFunction_ACN_27(const float Azimuth, const float Elevation)
	{
		const float CosElevation = FMath::Cos(Elevation);
		return MagicNumber_ACN_27 * FMath::Sin(3.0f * Azimuth) * (7.0f - 9.0f * FMath::Cos(2.0f * Elevation)) * CosElevation*CosElevation*CosElevation;
	}

	static const float MagicNumber_ACN_28 = 0.25f * FMath::Sqrt(105.0f);
	static float SHBasisFunction_ACN_28(const float Azimuth, const float Elevation)
	{
		const float SinElevation = FMath::Sin(Elevation);
		const float CosElevation = FMath::Cos(Elevation);
		return MagicNumber_ACN_28 * FMath::Sin(2.0f * Azimuth) * FMath::Sin(Elevation) * (3.0f * SinElevation*SinElevation - 1.0f) * CosElevation*CosElevation;
	}

	static const float MagicNumber_ACN_29 = FMath::Sqrt(15.0f) / 8.0f;
	static float SHBasisFunction_ACN_29(const float Azimuth, const float Elevation)
	{
		const float SinElevation = FMath::Sin(Elevation);
		const float SquaredSinElevation = SinElevation * SinElevation;
		return MagicNumber_ACN_29 * FMath::Sin(Azimuth) * FMath::Cos(Elevation) * (21.0f * SquaredSinElevation*SquaredSinElevation - 14.0f * SquaredSinElevation + 1.0f);
	}

	static float SHBasisFunction_ACN_30(const float Azimuth, const float Elevation)
	{
		const float SinElevation = FMath::Sin(Elevation);
		const float CubedSinElevation = SinElevation * SinElevation * SinElevation;
		return (63.0f * CubedSinElevation*SinElevation*SinElevation - 70.0f * CubedSinElevation + 15.0f * FMath::Sin(Elevation)) / 8.0f;
	}

	static const float MagicNumber_ACN_31 = FMath::Sqrt(15.0f) / 8.0f;
	static float SHBasisFunction_ACN_31(const float Azimuth, const float Elevation)
	{
		const float SinElevation = FMath::Sin(Elevation);
		const float SquaredSinElevation = SinElevation * SinElevation;
		return MagicNumber_ACN_31 * FMath::Cos(Azimuth) * FMath::Cos(Elevation) * (21.0f * SquaredSinElevation*SquaredSinElevation - 14.0f * SquaredSinElevation + 1.0f);
	}

	static const float MagicNumber_ACN_32 = 0.25f * FMath::Sqrt(105.0f);
	static float SHBasisFunction_ACN_32(const float Azimuth, const float Elevation)
	{
		const float SinElevation = FMath::Sin(Elevation);
		const float CosElevation = FMath::Cos(Elevation);
		return MagicNumber_ACN_32 * FMath::Cos(2.0f * Azimuth) * FMath::Sin(Elevation) * (3.0f * SinElevation*SinElevation - 1) * CosElevation;
	}

	static const float MagicNumber_ACN_33 = FMath::Sqrt(70.0f) / 32.0f;
	static float SHBasisFunction_ACN_33(const float Azimuth, const float Elevation)
	{
		const float CosElevation = FMath::Cos(Elevation);
		return MagicNumber_ACN_33 * FMath::Cos(3.0f * Azimuth) * (7.0f - 9.0f * FMath::Cos(2.0f * Elevation)) * CosElevation*CosElevation*CosElevation;
	}

	static const float MagicNumber_ACN_34 = 3.0f * FMath::Sqrt(35.0f) / 8.0f;
	static float SHBasisFunction_ACN_34(const float Azimuth, const float Elevation)
	{
		const float CosElevation = FMath::Cos(Elevation);
		return MagicNumber_ACN_34 * FMath::Cos(4.0f * Azimuth) * FMath::Sin(Elevation) * CosElevation*CosElevation*CosElevation*CosElevation;
	}

	static const float MagicNumber_ACN_35 = 3.0f * FMath::Sqrt(14.0f) / 16.0f;
	static float SHBasisFunction_ACN_35(const float Azimuth, const float Elevation)
	{
		const float CosElevation = FMath::Cos(Elevation);
		return MagicNumber_ACN_35 * FMath::Cos(5.0f * Azimuth) * CosElevation*CosElevation*CosElevation*CosElevation*CosElevation;
	}

#pragma endregion
} // namespace SHBasis

void FSphericalHarmonicCalculator::ComputeSoundfieldChannelGains(const int32 Order, const float Azimuth, const float Elevation, float* OutGains)
{
	check(OutGains);
	check(Order >= 0 && Order <= 5); // only have basis functions up to 5th order implemented

	switch (Order)
	{
		// lack of break; is intentional here.
		// (i.e. if Order is 2 we need to compute the gains for 1st and 0th order as well)
		case 5:
			OutGains[AmbiChanNumber::ACN_35] = SHBasis::SHBasisFunction_ACN_35(Azimuth, Elevation);
			OutGains[AmbiChanNumber::ACN_34] = SHBasis::SHBasisFunction_ACN_34(Azimuth, Elevation);
			OutGains[AmbiChanNumber::ACN_33] = SHBasis::SHBasisFunction_ACN_33(Azimuth, Elevation);
			OutGains[AmbiChanNumber::ACN_32] = SHBasis::SHBasisFunction_ACN_32(Azimuth, Elevation);
			OutGains[AmbiChanNumber::ACN_31] = SHBasis::SHBasisFunction_ACN_31(Azimuth, Elevation);
			OutGains[AmbiChanNumber::ACN_30] = SHBasis::SHBasisFunction_ACN_30(Azimuth, Elevation);
			OutGains[AmbiChanNumber::ACN_29] = SHBasis::SHBasisFunction_ACN_29(Azimuth, Elevation);
			OutGains[AmbiChanNumber::ACN_28] = SHBasis::SHBasisFunction_ACN_28(Azimuth, Elevation);
			OutGains[AmbiChanNumber::ACN_27] = SHBasis::SHBasisFunction_ACN_27(Azimuth, Elevation);
			OutGains[AmbiChanNumber::ACN_26] = SHBasis::SHBasisFunction_ACN_26(Azimuth, Elevation);
			OutGains[AmbiChanNumber::ACN_25] = SHBasis::SHBasisFunction_ACN_25(Azimuth, Elevation);
		case 4:
			OutGains[AmbiChanNumber::ACN_24] = SHBasis::SHBasisFunction_ACN_24(Azimuth, Elevation);
			OutGains[AmbiChanNumber::ACN_23] = SHBasis::SHBasisFunction_ACN_23(Azimuth, Elevation);
			OutGains[AmbiChanNumber::ACN_22] = SHBasis::SHBasisFunction_ACN_22(Azimuth, Elevation);
			OutGains[AmbiChanNumber::ACN_21] = SHBasis::SHBasisFunction_ACN_21(Azimuth, Elevation);
			OutGains[AmbiChanNumber::ACN_20] = SHBasis::SHBasisFunction_ACN_20(Azimuth, Elevation);
			OutGains[AmbiChanNumber::ACN_19] = SHBasis::SHBasisFunction_ACN_19(Azimuth, Elevation);
			OutGains[AmbiChanNumber::ACN_18] = SHBasis::SHBasisFunction_ACN_18(Azimuth, Elevation);
			OutGains[AmbiChanNumber::ACN_17] = SHBasis::SHBasisFunction_ACN_17(Azimuth, Elevation);
			OutGains[AmbiChanNumber::ACN_16] = SHBasis::SHBasisFunction_ACN_16(Azimuth, Elevation);
		case 3:
			OutGains[AmbiChanNumber::ACN_15] = SHBasis::SHBasisFunction_ACN_15(Azimuth, Elevation);
			OutGains[AmbiChanNumber::ACN_14] = SHBasis::SHBasisFunction_ACN_14(Azimuth, Elevation);
			OutGains[AmbiChanNumber::ACN_13] = SHBasis::SHBasisFunction_ACN_13(Azimuth, Elevation);
			OutGains[AmbiChanNumber::ACN_12] = SHBasis::SHBasisFunction_ACN_12(Azimuth, Elevation);
			OutGains[AmbiChanNumber::ACN_11] = SHBasis::SHBasisFunction_ACN_11(Azimuth, Elevation);
			OutGains[AmbiChanNumber::ACN_10] = SHBasis::SHBasisFunction_ACN_10(Azimuth, Elevation);
			OutGains[AmbiChanNumber::ACN_9] =  SHBasis::SHBasisFunction_ACN_9(Azimuth, Elevation);
		case 2:
			OutGains[AmbiChanNumber::ACN_8] = SHBasis::SHBasisFunction_ACN_8(Azimuth, Elevation);
			OutGains[AmbiChanNumber::ACN_7] = SHBasis::SHBasisFunction_ACN_7(Azimuth, Elevation);
			OutGains[AmbiChanNumber::ACN_6] = SHBasis::SHBasisFunction_ACN_6(Azimuth, Elevation);
			OutGains[AmbiChanNumber::ACN_5] = SHBasis::SHBasisFunction_ACN_5(Azimuth, Elevation);
			OutGains[AmbiChanNumber::ACN_4] = SHBasis::SHBasisFunction_ACN_4(Azimuth, Elevation);
		case 1:
			OutGains[AmbiChanNumber::ACN_3] = SHBasis::SHBasisFunction_ACN_3(Azimuth, Elevation);
			OutGains[AmbiChanNumber::ACN_2] = SHBasis::SHBasisFunction_ACN_2(Azimuth, Elevation);
			OutGains[AmbiChanNumber::ACN_1] = SHBasis::SHBasisFunction_ACN_1(Azimuth, Elevation);
		case 0:
			OutGains[AmbiChanNumber::ACN_0] = SHBasis::SHBasisFunction_ACN_0(Azimuth, Elevation);
			break;
	}
}

void FSphericalHarmonicCalculator::GenerateFirstOrderRotationMatrixGivenRadians(const float RotXRadians, const float RotYRadians, const float RotZRadians, FMatrix & OutMatrix)
{
	const float SinX = FMath::Sin(RotXRadians);
	const float SinY = FMath::Sin(RotYRadians);
	const float SinZ = FMath::Sin(RotZRadians);

	const float CosX = FMath::Cos(RotXRadians);
	const float CosY = FMath::Cos(RotYRadians);
	const float CosZ = FMath::Cos(RotZRadians);


	// Build out Rotation Matrix
	OutMatrix.SetIdentity();

	// row 0:
	/*	1.0f						0.0f															0.0f									0.0f									*/

	// row 1:
	/*	0.0f	*/	OutMatrix.M[1][1] = (CosX * CosZ) + (SinX * SinY * SinZ);		OutMatrix.M[1][2] = -SinX * CosY;		OutMatrix.M[1][3] = (CosX * SinZ) - (SinX * SinY * CosZ);

	// row 2:
	/*	0.0f	*/	OutMatrix.M[2][1] = (SinX * CosZ) - (CosX * SinY * SinZ);		OutMatrix.M[2][2] = CosX * CosY;		OutMatrix.M[2][3] = (CosX * SinY * CosZ) + (SinX * SinZ);

	// row 3:
	/*	0.0f	*/	OutMatrix.M[3][1] = -CosY * SinZ;								OutMatrix.M[3][2] = -SinY;				OutMatrix.M[3][3] = CosY * CosZ;
}

void FSphericalHarmonicCalculator::GenerateFirstOrderRotationMatrixGivenDegrees(const float RotXDegrees, const float RotYDegrees, const float RotZDegrees, FMatrix & OutMatrix)
{
	constexpr const float DEG_2_RAD = PI / 180.0f;
	GenerateFirstOrderRotationMatrixGivenRadians(RotXDegrees * DEG_2_RAD, RotYDegrees * DEG_2_RAD, RotZDegrees * DEG_2_RAD, OutMatrix);
}
