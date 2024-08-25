// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Job/Job.h"
#include "FxMat/FxMaterial.h"
#include "Model/Mix/MixUpdateCycle.h"

//////////////////////////////////////////////////////////////////////////
/// Shader
//////////////////////////////////////////////////////////////////////////

class FSH_FlatColorTexture : public FSH_Base
{
public:

	DECLARE_GLOBAL_SHADER(FSH_FlatColorTexture);
	SHADER_USE_PARAMETER_STRUCT(FSH_FlatColorTexture, FSH_Base);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(FLinearColor, Color)
		END_SHADER_PARAMETER_STRUCT()

};

typedef FxMaterial_Normal<VSH_Simple, FSH_FlatColorTexture>	Fx_FlatColorTexture;

/**
 * 
 */
class TEXTUREGRAPHENGINE_API T_FlatColorTexture
{
public:
	//////////////////////////////////////////////////////////////////////////
	/// Static functions
	//////////////////////////////////////////////////////////////////////////
	static BufferDescriptor			GetFlatColorDesc(FString name, BufferFormat InBufferFormat = BufferFormat::Byte);
	static TiledBlobPtr				Create(MixUpdateCyclePtr InCycle, BufferDescriptor DesiredOutputDesc, FLinearColor Color, int InTargetId);
};
