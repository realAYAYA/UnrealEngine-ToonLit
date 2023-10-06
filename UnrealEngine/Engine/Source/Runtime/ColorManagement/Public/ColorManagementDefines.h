// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"

namespace UE { namespace Color {

/** Increment upon breaking changes to the EEncoding enum. Note that changing this forces a rebuild of textures that rely on it. */
constexpr uint32 ENCODING_TYPES_VER = 4;

/** Increment upon breaking changes to the EColorSpace and EChromaticAdaptationMethod enums. Note that changing this forces a rebuild of textures that rely on it.*/
constexpr uint32 COLORSPACE_VER = 2;

/** List of available encodings/transfer functions.
* 
* NOTE: This list is replicated as a UENUM in TextureDefines.h, and both should always match.
*/
enum class EEncoding : uint8
{
	None = 0,
	Linear = 1,
	sRGB,
	ST2084,
	Gamma22,
	BT1886,
	Gamma26,
	Cineon,
	REDLog,
	REDLog3G10,
	SLog1,
	SLog2,
	SLog3,
	AlexaV3LogC,
	CanonLog,
	ProTune,
	VLog,
	Max,
};

/** List of available color spaces. (Increment COLORSPACE_VER upon breaking changes to the list.)
* 
* NOTE: This list is partially replicated as a UENUM in TextureDefines.h: any type exposed to textures should match the enum value below.
*/
enum class EColorSpace : uint8
{
	None = 0,
	sRGB = 1,
	Rec2020 = 2,
	ACESAP0 = 3,
	ACESAP1 = 4,
	P3DCI = 5,
	P3D65 = 6,
	REDWideGamut = 7,
	SonySGamut3 = 8,
	SonySGamut3Cine = 9,
	AlexaWideGamut = 10,
	CanonCinemaGamut = 11,
	GoProProtuneNative = 12,
	PanasonicVGamut = 13,
	PLASA_E1_54 = 14,
	Max,
};


/** List of available chromatic adaptation methods.
* 
* NOTE: This list is replicated as a UENUM in TextureDefines.h, and both should always match.
*/
enum class EChromaticAdaptationMethod : uint8
{
	None = 0,
	Bradford = 1,
	CAT02 = 2,
	Max,
};

/** List of standard white points. */
enum class EWhitePoint : uint8
{
	CIE1931_D65 = 0,
	ACES_D60 = 1,
	DCI_CalibrationWhite,
	Max,
};

} } // end namespace UE::Color
