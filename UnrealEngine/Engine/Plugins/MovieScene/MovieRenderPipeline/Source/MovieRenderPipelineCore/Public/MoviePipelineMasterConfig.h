// Copyright Epic Games, Inc. All Rights Reserved.

// HEADER_UNIT_SKIP - Deprecated
/**
 * DEPRECATED MoviePipelineMasterConfig.h Use MoviePipelinePrimaryConfig.h instead 
 */

#pragma once

#ifdef _MSC_VER
	#pragma message(__FILE__"(9): warning: use MoviePipelinePrimaryConfig.h instead of MoviePipelineMasterConfig.h")
#else
	#pragma message("#include MoviePipelinePrimaryConfig.h instead of MoviePipelineMasterConfig")
#endif

#include "MoviePipelinePrimaryConfig.h"

 // For backwards compatibility 5.2
UE_DEPRECATED(5.2, "UMoviePipelineMasterConfig is deprecated. Please use UMoviePipelinePrimaryConfig.")
typedef UMoviePipelinePrimaryConfig UMoviePipelineMasterConfig;