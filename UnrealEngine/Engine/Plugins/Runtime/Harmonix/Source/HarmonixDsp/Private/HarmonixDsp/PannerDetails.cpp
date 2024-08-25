// Copyright Epic Games, Inc. All Rights Reserved.

#include "HarmonixDsp/PannerDetails.h"

#include "Serialization/Archive.h"

bool FPannerDetails::Serialize(FArchive& Ar)
{
	uint8 Version = kVersion;
	Ar << Version;
	Ar << Mode;

	if (Mode == EPannerMode::DirectAssignment)
	{
		Ar << Detail.ChannelAssignment;
	}
	else
	{
		Ar << Detail.Pan;
		Ar << Detail.EdgeProximity;
	}
	return true;
}
