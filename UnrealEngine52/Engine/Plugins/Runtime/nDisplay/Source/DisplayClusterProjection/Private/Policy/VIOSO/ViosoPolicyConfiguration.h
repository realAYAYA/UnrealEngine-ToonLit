// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class IDisplayClusterViewport;


struct FViosoPolicyConfiguration
{
	FString OriginCompId;

	FString INIFile;
	FString ChannelName;

	// Calibration file and display segment with geometry
	FString CalibrationFile;
	int32     CalibrationIndex = -1;

	FMatrix BaseMatrix;

	//@todo add more vioso options, if required
	float Gamma = 1.f;

	bool Initialize(const TMap<FString, FString>& InParameters, IDisplayClusterViewport* InViewport);

	FString ToString() const;
};

