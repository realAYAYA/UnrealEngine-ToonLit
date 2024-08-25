// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "ColorManagementDefines.h"

namespace ElectraColorimetryUtils
{
	static constexpr uint8 DefaultMPEGColorPrimaries = 2;
	static constexpr uint8 DefaultMPEGMatrixCoefficients = 2;
	static constexpr uint8 DefaultMPEGTransferCharacteristics = 2;

	UE::Color::EColorSpace TranslateMPEGColorPrimaries(uint8 InPrimaries)
	{
		switch (InPrimaries)
		{
			case 1:				// Rec709
			case 2:				// unknown
				return UE::Color::EColorSpace::sRGB;
			case 9:				// Rec2020
				return UE::Color::EColorSpace::Rec2020;
			default:
				check(!"Unexpected MPEG color primaries value!");
		}
		return UE::Color::EColorSpace::sRGB;
	}

	UE::Color::EColorSpace TranslateMPEGMatrixCoefficients(uint8 InMatrixCoefficients)
	{
		switch (InMatrixCoefficients)
		{
			case 0:				// ID (RGB)
				return UE::Color::EColorSpace::None;
			case 1:				// Rec709
			case 2:				// unknown
				return UE::Color::EColorSpace::sRGB;
			case 9:				// Rec2020
				return UE::Color::EColorSpace::Rec2020;
			default:
				check(!"Unexpected MPEG color primaries value!");
		}
		return UE::Color::EColorSpace::sRGB;
	}

	UE::Color::EEncoding TranslateMPEGTransferCharacteristics(uint8 InTransferCharacteristics)
	{
		switch (InTransferCharacteristics)
		{
			case 1:
			case 6:
			case 14:
			case 15:
			case 2:				// unknown
				return UE::Color::EEncoding::sRGB;
			case 8:
				return UE::Color::EEncoding::Linear;
			case 16:
				return UE::Color::EEncoding::ST2084;
			case 18:
				return UE::Color::EEncoding::sRGB;	// using sRGB in place of HLG for now
			default:
				check(!"*** Unexpected MPEG color transfer characteristics!");
		}
		return UE::Color::EEncoding::sRGB;
	}
};
