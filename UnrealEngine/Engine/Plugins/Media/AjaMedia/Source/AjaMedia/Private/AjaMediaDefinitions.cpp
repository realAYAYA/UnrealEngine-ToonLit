// Copyright Epic Games, Inc. All Rights Reserved.

#include "AjaMediaDefinitions.h"

#include "AjaLib.h"

namespace UE::AjaMedia
{
	FAjaMediaHDROptions MakeAjaMediaHDROptions(const AJA::FAjaHDROptions& HDROptions)
	{
		FAjaMediaHDROptions HDRMetadata;

		HDRMetadata.Gamut = (EAjaHDRMetadataGamut)HDROptions.Gamut;
		HDRMetadata.EOTF = (EAjaHDRMetadataEOTF)HDROptions.EOTF;

		return HDRMetadata;
	}

	AJA::FAjaHDROptions MakeAjaHDROptions(const FAjaMediaHDROptions& HDROptions)
	{
		AJA::FAjaHDROptions HDRMetadata;

		HDRMetadata.Gamut = (AJA::EAjaHDRMetadataGamut)HDROptions.Gamut;
		HDRMetadata.EOTF = (AJA::EAjaHDRMetadataEOTF)HDROptions.EOTF;

		return HDRMetadata;
	}
}
