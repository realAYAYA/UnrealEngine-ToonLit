// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ScreenPass.h"

class FVisualizeTexturePresent
{
public:
	/** Starts texture visualization capture. */
	static void OnStartRender(const FViewInfo& View);

	/** Present the visualize texture tool on screen. */
	static void PresentContent(FRDGBuilder& GraphBuilder, const FViewInfo& View, FScreenPassRenderTarget Output);
};