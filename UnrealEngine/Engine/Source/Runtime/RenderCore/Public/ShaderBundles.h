// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GlobalShader.h"
#include "HAL/Platform.h"
#include "Misc/AssertionMacros.h"
#include "RHICommandList.h"
#include "RHIDefinitions.h"
#include "Serialization/MemoryLayout.h"
#include "Shader.h"
#include "ShaderCore.h"
#include "ShaderParameterMacros.h"
#include "ShaderParameterStruct.h"
#include "ShaderParameterUtils.h"
#include "ShaderParameters.h"
#include "ShaderPermutation.h"

class FRHIShaderBundle;

class FDispatchShaderBundleCS : public FGlobalShader
{
	DECLARE_EXPORTED_GLOBAL_SHADER(FDispatchShaderBundleCS, RENDERCORE_API);

public:

	FDispatchShaderBundleCS() = default;
	FDispatchShaderBundleCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		// Platforms with support for root constants will not have a bind point for this parameter
		RootConstantsParam.Bind(Initializer.ParameterMap, TEXT("UERootConstants"), SPF_Optional);

		RecordArgBufferParam.Bind(Initializer.ParameterMap, TEXT("RecordArgBuffer"), SPF_Mandatory);
		RecordDataBufferParam.Bind(Initializer.ParameterMap, TEXT("RecordDataBuffer"), SPF_Mandatory);
		RWExecutionBufferParam.Bind(Initializer.ParameterMap, TEXT("RWExecutionBuffer"), SPF_Mandatory);
	}

	static const uint32 ThreadGroupSizeX = 64;

	LAYOUT_FIELD(FShaderParameter, RootConstantsParam);
	LAYOUT_FIELD(FShaderResourceParameter, RecordArgBufferParam);
	LAYOUT_FIELD(FShaderResourceParameter, RecordDataBufferParam);
	LAYOUT_FIELD(FShaderResourceParameter, RWExecutionBufferParam);

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);
};
