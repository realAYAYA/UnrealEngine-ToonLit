// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "PostProcess/PostProcessCompositePrimitivesCommon.h"

#if WITH_EDITOR
FScreenPassTexture AddEditorPrimitivePass(FRDGBuilder& GraphBuilder, const FViewInfo& View, const FCompositePrimitiveInputs& Inputs, FInstanceCullingManager& InstanceCullingManager);
#endif
