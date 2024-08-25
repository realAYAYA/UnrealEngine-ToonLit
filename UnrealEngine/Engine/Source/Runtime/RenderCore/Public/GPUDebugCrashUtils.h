// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

/*=============================================================================
	GPUDebugCrashUtils.h: Utilities for crashing the GPU in various ways on purpose.
	=============================================================================*/
#include "RenderGraphBuilder.h"

extern RENDERCORE_API void ScheduleGPUDebugCrash(FRDGBuilder& GraphBuilder);
