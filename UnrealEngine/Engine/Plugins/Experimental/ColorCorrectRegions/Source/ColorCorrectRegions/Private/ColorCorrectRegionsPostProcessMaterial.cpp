// Copyright Epic Games, Inc. All Rights Reserved.

#include "ColorCorrectRegionsPostProcessMaterial.h"
#include "MaterialShader.h"
#include "MaterialShaderType.h"
#include "Materials/Material.h"
#include "GlobalShader.h"
#include "TextureResource.h"
#include "ShaderCore.h"
#include "ShaderParameterMacros.h"
#include "CoreMinimal.h"
#include "CommonRenderResources.h"
#include "RenderGraph.h"
#include "RendererInterface.h"
#include "SceneRendering.h"
#include "PostProcess/PostProcessMaterial.h"

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FCCRRegionDataInputParameter, "RegionData");
IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FCCRColorCorrectParameter, "ColorCorrectBase");
IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FCCRColorCorrectShadowsParameter, "ColorCorrectShadows");
IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FCCRColorCorrectMidtonesParameter, "ColorCorrectMidtones");
IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FCCRColorCorrectHighlightsParameter, "ColorCorrectHighlights");


IMPLEMENT_GLOBAL_SHADER(FColorCorrectRegionMaterialVS, "/ColorCorrectRegionsShaders/Private/ColorCorrectRegionsShader.usf", "MainVS", SF_Vertex);
IMPLEMENT_GLOBAL_SHADER(FColorCorrectRegionMaterialPS, "/ColorCorrectRegionsShaders/Private/ColorCorrectRegionsShader.usf", "MainPS", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FColorCorrectWindowMaterialPS, "/ColorCorrectRegionsShaders/Private/ColorCorrectRegionsShader.usf", "MainPS", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FColorCorrectScreenPassVS, "/ColorCorrectRegionsShaders/Private/ColorCorrectRegionsScreenPass.usf", "MainVS", SF_Vertex);
IMPLEMENT_GLOBAL_SHADER(FClearRectPS, "/ColorCorrectRegionsShaders/Private/ColorCorrectRegionsScreenPass.usf", "MainPS", SF_Pixel);

IMPLEMENT_GLOBAL_SHADER(FCCRStencilMergerPS, "/ColorCorrectRegionsShaders/Private/ColorCorrectRegionsStencilMerger.usf", "MainPS", SF_Pixel);