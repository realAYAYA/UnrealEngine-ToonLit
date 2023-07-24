// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GlobalDistanceFieldParameters.h"
#include "RenderGraphBuilder.h"

class FDistanceFieldSceneData;
class FDistanceFieldObjectBufferParameters;
class FDistanceFieldAtlasParameters;

namespace FNiagaraDistanceFieldHelper
{
	NIAGARASHADER_API void SetGlobalDistanceFieldParameters(const FGlobalDistanceFieldParameterData* OptionalParameterData, FGlobalDistanceFieldParameters2& ShaderParameters);
	NIAGARASHADER_API void SetMeshDistanceFieldParameters(FRDGBuilder& GraphBuilder, const FDistanceFieldSceneData* OptionalDistanceFieldData, FDistanceFieldObjectBufferParameters& ObjectShaderParameters, FDistanceFieldAtlasParameters& AtlasShaderParameters, FRHIShaderResourceView* DummyFloat4Buffer);
}
