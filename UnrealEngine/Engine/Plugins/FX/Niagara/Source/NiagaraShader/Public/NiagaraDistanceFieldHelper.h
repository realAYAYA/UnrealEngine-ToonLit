// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_3
#include "GlobalDistanceFieldParameters.h"
#include "RenderGraphBuilder.h"
#endif

class FRDGBuilder;
class FGlobalDistanceFieldParameterData;
class FGlobalDistanceFieldParameters2;

namespace FNiagaraDistanceFieldHelper
{
	NIAGARASHADER_API void SetGlobalDistanceFieldParameters(const FGlobalDistanceFieldParameterData* OptionalParameterData, FGlobalDistanceFieldParameters2& ShaderParameters);
}
