// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "ShaderParameterMacros.h"

BEGIN_SHADER_PARAMETER_STRUCT(FTileInfo, )
	SHADER_PARAMETER(float, TileX)
	SHADER_PARAMETER(float, TileCountX)
	SHADER_PARAMETER(float, TileWidth)
	SHADER_PARAMETER(float, TileY)
	SHADER_PARAMETER(float, TileCountY)
	SHADER_PARAMETER(float, TileHeight)
END_SHADER_PARAMETER_STRUCT()

