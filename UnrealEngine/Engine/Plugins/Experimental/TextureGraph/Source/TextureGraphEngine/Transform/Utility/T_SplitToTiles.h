// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "ShaderCompilerCore.h"
#include "Job/Job.h"
#include "FxMat/FxMaterial.h"
#include "Model/Mix/MixUpdateCycle.h"
#include <DataDrivenShaderPlatformInfo.h>

//////////////////////////////////////////////////////////////////////////
/// Shader
//////////////////////////////////////////////////////////////////////////

class CSH_SplitToTiles : public CmpSH_Base<1, 1, 1>
{
public:
	DECLARE_GLOBAL_SHADER(CSH_SplitToTiles);
	SHADER_USE_PARAMETER_STRUCT(CSH_SplitToTiles, CmpSH_Base);
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT(FTileInfo, TileInfo)
		SHADER_PARAMETER_TEXTURE(Texture2D, SourceTexture)
		SHADER_PARAMETER_UAV(RWTexture2D<float4>, Result)
	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) {
		return IsPCPlatform(Parameters.Platform) && EnumHasAllFlags(Parameters.Flags, EShaderPermutationFlags::HasEditorOnlyData) && 
			IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

/**
 * SplitToTiles Transform
 */
class TEXTUREGRAPHENGINE_API T_SplitToTiles
{
public:
	T_SplitToTiles();
	~T_SplitToTiles();

	//////////////////////////////////////////////////////////////////////////
	/// Static functions
	//////////////////////////////////////////////////////////////////////////
	
	static TiledBlobPtr				Create(MixUpdateCyclePtr Cycle, int32 TargetId, TiledBlobPtr SourceTex);
};
