// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "PostProcess/PostProcessCompositePrimitivesCommon.h"

#if UE_ENABLE_DEBUG_DRAWING
FScreenPassTexture AddDebugPrimitivePass(FRDGBuilder& GraphBuilder, const FViewInfo& View, const FCompositePrimitiveInputs& Inputs);
#endif
