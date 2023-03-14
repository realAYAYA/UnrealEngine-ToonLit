// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================================
	HoloLensSurvey.h: HoloLens platform hardware-survey classes
==============================================================================================*/

#pragma once
#include "GenericPlatform/GenericPlatformSurvey.h"

/**
* HoloLens implementation of FGenericPlatformSurvey
**/
struct FHoloLensPlatformSurvey : public FGenericPlatformSurvey
{
	// default implementation for now
};

typedef FHoloLensPlatformSurvey FPlatformSurvey;