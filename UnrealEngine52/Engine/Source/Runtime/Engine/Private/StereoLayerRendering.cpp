// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ScreenRendering.cpp: Screen rendering implementation.
=============================================================================*/

#include "StereoLayerRendering.h"
#include "Misc/DelayedAutoRegister.h"

IMPLEMENT_TYPE_LAYOUT(FStereoLayerPS_Base);
IMPLEMENT_SHADER_TYPE(,FStereoLayerVS,TEXT("/Engine/Private/StereoLayerShader.usf"),TEXT("MainVS"),SF_Vertex);
IMPLEMENT_SHADER_TYPE(,FStereoLayerPS,TEXT("/Engine/Private/StereoLayerShader.usf"),TEXT("MainPS_Texture2D"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(,FStereoLayerPS_External,TEXT("/Engine/Private/StereoLayerShader.usf"),TEXT("MainPS_TextureExternal"),SF_Pixel);
