// Copyright Epic Games, Inc. All Rights Reserved.

#include "RenderGraphFwd.h"

class FViewInfo;
class FSkyLightSceneProxy;
class FRDGBuilder;
struct FScreenPassTexture;

FScreenPassTexture ProcessAndRenderIlluminanceMeter(FRDGBuilder& GraphBuilder, const FViewInfo& View, FScreenPassTexture& ScreenPassSceneColor);
