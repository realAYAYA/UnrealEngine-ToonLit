// Copyright Epic Games, Inc. All Rights Reserved.

#include "TransferFunctions.h"
#include "Logging/LogMacros.h"

DEFINE_LOG_CATEGORY_STATIC(LogTransferFunctions, Log, All);

namespace UE { namespace Color {

FORCEINLINE TFunction<float(float)> GetTransferFunction(EEncoding SourceEncoding, bool bIsEncode)
{
	switch (SourceEncoding)
	{
	case EEncoding::None:        return &Linear;
	case EEncoding::Linear:      return &Linear;
	case EEncoding::sRGB:        return bIsEncode ? &EncodeSRGB : &DecodeSRGB;
	case EEncoding::ST2084:      return bIsEncode ? &EncodeST2084 : &DecodeST2084;
	case EEncoding::Gamma22:     return bIsEncode ? &EncodeGamma22 : &DecodeGamma22;
	case EEncoding::BT1886:      return bIsEncode ? &EncodeBT1886 : &DecodeBT1886;
	case EEncoding::Gamma26:     return bIsEncode ? &EncodeGamma26 : &DecodeGamma26;
	case EEncoding::Cineon:      return bIsEncode ? &EncodeCineon : &DecodeCineon;
	case EEncoding::REDLog:      return bIsEncode ? &EncodeREDLog : &DecodeREDLog;
	case EEncoding::REDLog3G10:  return bIsEncode ? &EncodeREDLog3G10 : &DecodeREDLog3G10;
	case EEncoding::SLog1:       return bIsEncode ? &EncodeSLog1 : &DecodeSLog1;
	case EEncoding::SLog2:       return bIsEncode ? &EncodeSLog2 : &DecodeSLog2;
	case EEncoding::SLog3:       return bIsEncode ? &EncodeSLog3 : &DecodeSLog3;
	case EEncoding::AlexaV3LogC: return bIsEncode ? &EncodeArriAlexaV3LogC : &DecodeArriAlexaV3LogC;
	case EEncoding::CanonLog:    return bIsEncode ? &EncodeCanonLog : &DecodeCanonLog;
	case EEncoding::ProTune:     return bIsEncode ? &EncodeGoProProTune : &DecodeGoProProTune;
	case EEncoding::VLog:        return bIsEncode ? &EncodePanasonicVLog : &DecodePanasonicVLog;
	default:
		check(false);
		break;
	}

	UE_LOG(LogTransferFunctions, Warning, TEXT("Failed to find valid transfer function for enum value %d."), int(SourceEncoding));

	return nullptr;
}

TFunction<float(float)> GetEncodeFunction(EEncoding SourceEncoding)
{
	return GetTransferFunction(SourceEncoding, true);
}

TFunction<float(float)> GetDecodeFunction(EEncoding SourceEncoding)
{
	return GetTransferFunction(SourceEncoding, false);
}

float Encode(EEncoding SourceEncoding, float Value)
{
	if (TFunction<float(float)> TransferFn = GetEncodeFunction(SourceEncoding))
	{
		return TransferFn(Value);
	}
	else
	{
		return Value;
	}
}

float Decode(EEncoding SourceEncoding, float Value)
{
	if (TFunction<float(float)> TransferFn = GetDecodeFunction(SourceEncoding))
	{
		return TransferFn(Value);
	}
	else
	{
		return Value;
	}
}

TFunction<FLinearColor(const FLinearColor&)> GetColorEncodeFunction(EEncoding SourceEncoding)
{
	TFunction<float(float)> TransferFn = GetEncodeFunction(SourceEncoding);

	if (!TransferFn)
	{
		return nullptr;
	}

	return [TransferFn](const FLinearColor& Color) -> FLinearColor
	{
		return FLinearColor(TransferFn(Color.R), TransferFn(Color.G), TransferFn(Color.B), Color.A);
	};
}

TFunction<FLinearColor(const FLinearColor&)> GetColorDecodeFunction(EEncoding SourceEncoding)
{
	TFunction<float(float)> TransferFn = GetDecodeFunction(SourceEncoding);

	if (!TransferFn)
	{
		return nullptr;
	}

	return [TransferFn](const FLinearColor& Color) -> FLinearColor
	{
		return FLinearColor(TransferFn(Color.R), TransferFn(Color.G), TransferFn(Color.B), Color.A);
	};
}

FLinearColor Encode(EEncoding SourceEncoding, const FLinearColor& Color)
{
	if (TFunction<FLinearColor(const FLinearColor&)> TransferFn = GetColorEncodeFunction(SourceEncoding))
	{
		return TransferFn(Color);
	}
	else
	{
		return Color;
	}
}

FLinearColor Decode(EEncoding SourceEncoding, const FLinearColor& Color)
{
	if (TFunction<FLinearColor(const FLinearColor&)> TransferFn = GetColorDecodeFunction(SourceEncoding))
	{
		return TransferFn(Color);
	}
	else
	{
		return Color;
	}
}

} } // end namespace UE::Color
