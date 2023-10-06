// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ColorManagementDefines.h"
#include "CoreTypes.h"
#include "GenericPlatform/GenericPlatformMath.h"
#include "Math/Color.h"
#include "Math/UnrealMathUtility.h"
#include "Templates/Function.h"

namespace UE { namespace Color {

FORCEINLINE float Linear(float Value)
{
	return Value;
}

/**
* Encode value to sRGB.
*
* @return float encoded value.
*/
FORCEINLINE float EncodeSRGB(float Value)
{
	if (Value <= 0.04045f / 12.92f)
	{
		return Value * 12.92f;
	}
	else
	{
		return  FGenericPlatformMath::Pow(Value, (1.0f / 2.4f)) * 1.055f - 0.055f;
	}
}

/**
* Decode value with an sRGB encoding.
*
* @return float decoded value.
*/
FORCEINLINE float DecodeSRGB(float Value)
{
	if (Value <= 0.04045f)
	{
		return Value / 12.92f;
	}
	else
	{
		return  FGenericPlatformMath::Pow((Value + 0.055f) / 1.055f, 2.4f);
	}
}

/**
* Encode value to SMPTE ST 2084:2014.
*
* @return float encoded value.
*/
FORCEINLINE float EncodeST2084(float Value)
{
	const float Lp = 10000.0f;
	const float m1 = 2610 / 4096.0f * (1.0f / 4.0f);
	const float m2 = 2523 / 4096.0f * 128.0f;
	const float c1 = 3424 / 4096.0f;
	const float c2 = 2413 / 4096.0f * 32.f;
	const float c3 = 2392 / 4096.0f * 32.f;

	Value = FGenericPlatformMath::Pow(Value / Lp, m1);
	return FGenericPlatformMath::Pow((c1 + c2 * Value) / (c3 * Value + 1), m2);
}

/**
* Decode value with a SMPTE ST 2084:2014 encoding.
*
* @return float decoded value.
*/
FORCEINLINE float DecodeST2084(float Value)
{
	const float Lp = 10000.0f;
	const float m1 = 2610 / 4096.0f * (1.0f / 4.0f);
	const float m2 = 2523 / 4096.0f * 128.0f;
	const float c1 = 3424 / 4096.0f;
	const float c2 = 2413 / 4096.0f * 32.f;
	const float c3 = 2392 / 4096.0f * 32.f;

	const float Vp = FGenericPlatformMath::Pow(Value, 1.0f / m2);
	Value = FGenericPlatformMath::Max(0.0f, Vp - c1);
	return FGenericPlatformMath::Pow((Value / (c2 - c3 * Vp)), 1.0f / m1) * Lp;
}

/**
* Encode value to Gamma 2.2.
*
* @return float encoded value.
*/
FORCEINLINE float EncodeGamma22(float Value)
{
	return FGenericPlatformMath::Pow(Value, 1.0f / 2.2f);
}


/**
* Decode value with a Gamma 2.2 encoding.
*
* @return float decoded value.
*/
FORCEINLINE float DecodeGamma22(float Value)
{
	return FGenericPlatformMath::Pow(Value, 2.2f);
}

/**
* Encode value to ITU-R BT.1886.
*
* @return float encoded value.
*/
FORCEINLINE float EncodeBT1886(float Value)
{
	const float L_B = 0;
	const float L_W = 1;
	float Gamma = 2.40f;
	float GammaInv = 1.0f / Gamma;
	float N = FGenericPlatformMath::Pow(L_W, GammaInv) - FGenericPlatformMath::Pow(L_B, GammaInv);
	float A = FGenericPlatformMath::Pow(N, Gamma);
	float B = FGenericPlatformMath::Pow(L_B, GammaInv) / N;
	return FGenericPlatformMath::Pow(Value / A, GammaInv) - B;
}

/**
* Encode value to Gamma 2.6.
*
* @return float encoded value.
*/
FORCEINLINE float EncodeGamma26(float Value)
{
	return FGenericPlatformMath::Pow(Value, 1.0f / 2.6f);
}


/**
* Decode value with a Gamma 2.6 encoding.
*
* @return float decoded value.
*/
FORCEINLINE float DecodeGamma26(float Value)
{
	return FGenericPlatformMath::Pow(Value, 2.6f);
}

/**
* Decode value with an ITU-R BT.1886 encoding.
*
* @return float decoded value.
*/
FORCEINLINE float DecodeBT1886(float Value)
{
	const float L_B = 0;
	const float L_W = 1;
	float Gamma = 2.40f;
	float GammaInv = 1.0f / Gamma;
	float N = FGenericPlatformMath::Pow(L_W, GammaInv) - FGenericPlatformMath::Pow(L_B, GammaInv);
	float A = FGenericPlatformMath::Pow(N, Gamma);
	float B = FGenericPlatformMath::Pow(L_B, GammaInv) / N;
	return A * FGenericPlatformMath::Pow(FGenericPlatformMath::Max(Value + B, 0.0f), Gamma);
}

/**
* Encode value to Cineon.
*
* @return float encoded value.
*/
FORCEINLINE float EncodeCineon(float Value)
{
	const float BlackOffset = FGenericPlatformMath::Pow(10.0f, (95.0f - 685.0f) / 300.0f);
	return (685.0f + 300.0f * FGenericPlatformMath::LogX(10.0f, Value * (1.0f - BlackOffset) + BlackOffset)) / 1023.0f;
}

/**
* Decode value with a Cineon encoding.
*
* @return float decoded value.
*/
FORCEINLINE float DecodeCineon(float Value)
{
	const float BlackOffset = FGenericPlatformMath::Pow(10.0f, (95.0f - 685.0f) / 300.0f);
	return (FGenericPlatformMath::Pow(10.0f, (1023.0f * Value - 685.0f) / 300.0f) - BlackOffset) / (1.0f - BlackOffset);
}

/**
* Encode value to RED Log.
*
* @return float encoded value.
*/
FORCEINLINE float EncodeREDLog(float Value)
{
	const float BlackOffset = FGenericPlatformMath::Pow(10.0f, (0.0f - 1023.0f) / 511.0f);
	return (1023.0f + 511.0f * FGenericPlatformMath::LogX(10.0f, Value * (1.0f - BlackOffset) + BlackOffset)) / 1023.0f;
}

/**
* Decode value with a RED Log encoding.
*
* @return float decoded value.
*/
FORCEINLINE float DecodeREDLog(float Value)
{
	const float BlackOffset = FGenericPlatformMath::Pow(10.0f, (0.0f - 1023.0f) / 511.0f);
	return (FGenericPlatformMath::Pow(10.0f, (1023.0f * Value - 1023.0f) / 511.0f) - BlackOffset) / (1.0f - BlackOffset);
}

/**
* Encode value to RED Log3G10.
*
* @return float encoded value.
*/
FORCEINLINE float EncodeREDLog3G10(float Value)
{
	const float A = 0.224282f;
	const float B = 155.975327f;
	const float C = 0.01f;
	const float G = 15.1927f;

	Value += C;

	if (Value < 0.0f)
	{
		return Value * G;
	}
	else
	{
		return FGenericPlatformMath::Sign(Value) * A * FGenericPlatformMath::LogX(10.0f, B * FGenericPlatformMath::Abs(Value) + 1.0f);
	}
}

/**
* Decode value with a RED Log3G10 encoding.
*
* @return float decoded value.
*/
FORCEINLINE float DecodeREDLog3G10(float Value)
{
	const float A = 0.224282f;
	const float B = 155.975327f;
	const float C = 0.01f;
	const float G = 15.1927f;

	if (Value < 0.0f)
	{
		Value /= G;
	}
	else
	{
		Value = FGenericPlatformMath::Sign(Value) * (FGenericPlatformMath::Pow(10.0f, FGenericPlatformMath::Abs(Value) / A ) - 1.0f) / B;
	}

	return Value - C;
}

/**
* Encode value to Sony S-Log1.
*
* @return float encoded value.
*/
FORCEINLINE float EncodeSLog1(float Value)
{
	Value /= 0.9f;
	Value = 0.432699f * FGenericPlatformMath::LogX(10.0f, Value + 0.037584f) + 0.616596f + 0.03f;
	return (Value * 219.0f + 16.0f) * 4.0f / 1023.0f;
}

/**
* Decode value with a Sony S-Log1 encoding.
*
* @return float decoded value.
*/
FORCEINLINE float DecodeSLog1(float Value)
{
	Value = ((Value * 1023.f) / 4.0f - 16.0f) / 219.0f;
	Value = FGenericPlatformMath::Pow(10.0f, (Value - 0.616596f - 0.03f) / 0.432699f) - 0.037584f;
	return Value * 0.9f;
}

/**
* Encode value to Sony S-Log2.
*
* @return float encoded value.
*/
FORCEINLINE float EncodeSLog2(float Value)
{
	if (Value >= 0.0f)
	{
		return (64.0f + 876.0f * (0.432699f * FGenericPlatformMath::LogX(10.0f, 155.0f * Value / 197.1f + 0.037584f) + 0.646596f)) / 1023.f;
	}
	else
	{
		return (64.0f +876.0f * (Value * 3.53881278538813f / 0.9f) + 0.646596f + 0.030001222851889303f) / 1023.f;
	}
}

/**
* Decode value with a Sony S-Log2 encoding.
*
* @return float decoded value.
*/
FORCEINLINE float DecodeSLog2(float Value)
{
	if (Value >= (64.f + 0.030001222851889303f * 876.f) / 1023.f)
	{
		return 197.1f * (FGenericPlatformMath::Pow(10.0f, ((Value * 1023.f - 64.f) / 876.f - 0.646596f) / 0.432699f) - 0.037584f) / 155.f;
	}
	else
	{
		return 0.9f * ((Value * 1023.f - 64.f) / 876.f - 0.030001222851889303f) / 3.53881278538813f;
	}
}

/**
* Encode value to Sony S-Log3.
*
* @return float encoded value.
*/
FORCEINLINE float EncodeSLog3(float Value)
{
	if (Value >= 0.01125000f)
	{
		return (420.0f + FGenericPlatformMath::LogX(10.0f, (Value + 0.01f) / 0.19f) * 261.5f) / 1023.0f;
	}
	else
	{
		return (Value * 76.2102946929f / 0.01125f + 95.0f) / 1023.0f;
	}
}

/**
* Decode value with a Sony S-Log3 encoding.
*
* @return float decoded value.
*/
FORCEINLINE float DecodeSLog3(float Value)
{
	if (Value >= 171.2102946929f / 1023.0f)
	{
		return (FGenericPlatformMath::Pow(10.0f, (Value * 1023.0f - 420.f) / 261.5f)) * 0.19f - 0.01f;
	}
	else
	{
		return (Value * 1023.0f - 95.0f) * 0.01125000f / (171.2102946929f - 95.0f);
	}
}

/**
* Encode value to ARRI Alexa LogC.
*
* @return float encoded value.
*/
FORCEINLINE float EncodeArriAlexaV3LogC(float Value)
{
	const float cut = 0.010591f;
	const float a = 5.555556f;
	const float b = 0.052272f;
	const float c = 0.247190f;
	const float d = 0.385537f;
	const float e = 5.367655f;
	const float f = 0.092809f;

	if (Value > cut)
	{
		return c * FGenericPlatformMath::LogX(10.0f, a * Value + b) + d;
	}
	else
	{
		return e * Value + f;
	}
}

/**
* Decode value with an ARRI Alexa LogC encoding.
*
* @return float decoded value.
*/
FORCEINLINE float DecodeArriAlexaV3LogC(float Value)
{
	const float cut = 0.010591f;
	const float a = 5.555556f;
	const float b = 0.052272f;
	const float c = 0.247190f;
	const float d = 0.385537f;
	const float e = 5.367655f;
	const float f = 0.092809f;
	
	if (Value > e * cut + f)
	{
		return (FGenericPlatformMath::Pow(10.0f, (Value - d) / c) - b) / a;
	}
	else
	{
		return (Value - f) / e;
	}
}

/**
* Encode value to Canon Log.
*
* @return float encoded value.
*/
FORCEINLINE float EncodeCanonLog(float Value)
{
	if (Value < 0.0f)
	{
		return -(0.529136f * (FGenericPlatformMath::LogX(10.0f, -Value * 10.1596f + 1.0f)) - 0.0730597f);
	}
	else
	{
		return 0.529136f * FGenericPlatformMath::LogX(10.0f, 10.1596f * Value + 1.0f) + 0.0730597f;
	}
}


/**
* Decode value with a Canon Log encoding.
*
* @return float decoded value.
*/
FORCEINLINE float DecodeCanonLog(float Value)
{
	if (Value < 0.0730597f)
	{
		return -(FGenericPlatformMath::Pow(10.0f, (0.0730597f - Value) / 0.529136f) - 1.0f) / 10.1596f;
	}
	else
	{
		return (FGenericPlatformMath::Pow(10.0f, (Value - 0.0730597f) / 0.529136f) - 1.0f) / 10.1596f;
	}
}

/**
* Encode value to GoPro ProTune.
*
* @return float encoded value.
*/
FORCEINLINE float EncodeGoProProTune(float Value)
{
	return FGenericPlatformMath::Loge(Value * 112.f + 1.0f) / FGenericPlatformMath::Loge(113.0f);
}

/**
* Decode value with a GoPro ProTune encoding.
*
* @return float decoded value.
*/
FORCEINLINE float DecodeGoProProTune(float Value)
{
	return (FGenericPlatformMath::Pow(113.f, Value) - 1.0f) / 112.f;
}

/**
* Encode value to Panasonic V-Log.
*
* @return float encoded value.
*/
FORCEINLINE float EncodePanasonicVLog(float Value)
{
	const float b = 0.00873f;
	const float c = 0.241514f;
	const float d = 0.598206f;

	if( Value < 0.01f)
	{
		return 5.6f * Value + 0.125f;
	}
	else
	{
		return c * FGenericPlatformMath::LogX(10.0f, Value + b) + d;
	}
}

/**
* Decode value with a Panasonic V-Log encoding.
*
* @return float decoded value.
*/
FORCEINLINE float DecodePanasonicVLog(float Value)
{
	const float b = 0.00873f;
	const float c = 0.241514f;
	const float d = 0.598206f;

	if (Value < 0.181f)
	{
		return (Value - 0.125f) / 5.6f;
	}
	else
	{
		return FGenericPlatformMath::Pow(10.0f, (Value - d) / c) - b;
	}
}

/** Get the encode function that matches the encoding type. */
COLORMANAGEMENT_API TFunction<float(float)> GetEncodeFunction(EEncoding Encoding);

/** Get the decode function that matches the encoding type. */
COLORMANAGEMENT_API TFunction<float(float)> GetDecodeFunction(EEncoding Encoding);

/** Encode a value based on the specified encoding type. */
COLORMANAGEMENT_API float Encode(EEncoding Encoding, float Value);

/** Decode a value based on the specified encoding type. */
COLORMANAGEMENT_API float Decode(EEncoding Encoding, float Value);

/** Get the encode function that matches the encoding type. */
COLORMANAGEMENT_API TFunction<FLinearColor(const FLinearColor&)> GetColorEncodeFunction(EEncoding Encoding);

/** Get the decode function that matches the encoding type. */
COLORMANAGEMENT_API TFunction<FLinearColor(const FLinearColor&)> GetColorDecodeFunction(EEncoding Encoding);

/** Encode a color based on the specified encoding type. */
COLORMANAGEMENT_API FLinearColor Encode(EEncoding Encoding, const FLinearColor& Color);

/** Decode a color based on the specified encoding type. */
COLORMANAGEMENT_API FLinearColor Decode(EEncoding Encoding, const FLinearColor& Color);

} } // end namespace UE::Color
