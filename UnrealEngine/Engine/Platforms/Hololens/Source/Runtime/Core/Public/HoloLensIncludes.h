// Copyright Epic Games, Inc. All Rights Reserved.

/*================================================================================
	HoloLensIncludes.h: Includes the platform specific headers for HoloLens
==================================================================================*/

#pragma once

// Set up compiler pragmas, etc
#include "HoloLensCompilerSetup.h"

// include platform implementations
#include "HoloLensMemory.h"
#include "HoloLensString.h"
#include "HoloLensMisc.h"
#include "HoloLensStackWalk.h"
#include "HoloLensMath.h"
#include "HoloLensTime.h"
#include "HoloLensProcess.h"
#include "HoloLensOutputDevices.h"
#include "HoloLensAtomics.h"
#include "HoloLensTLS.h"
#include "HoloLensSplash.h"
#include "HoloLensSurvey.h"

typedef FGenericPlatformRHIFramePacer FPlatformRHIFramePacer;

typedef FGenericPlatformAffinity FPlatformAffinity;

#include "HoloLensProperties.h"
