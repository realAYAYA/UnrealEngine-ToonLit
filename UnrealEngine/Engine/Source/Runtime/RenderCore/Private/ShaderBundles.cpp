// Copyright Epic Games, Inc. All Rights Reserved.

#include "ShaderBundles.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "RenderGraph.h"
#include "RenderGraphDefinitions.h"
#include "ShaderParameterMacros.h"
#include "RenderGraphFwd.h"
#include "ShaderCompilerCore.h"

IMPLEMENT_GLOBAL_SHADER(FDispatchShaderBundleCS, "/Engine/Private/ShaderBundleDispatch.usf", "DispatchShaderBundleEntry", SF_Compute);

bool FDispatchShaderBundleCS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return RHISupportsShaderBundleDispatch(Parameters.Platform);
}

void FDispatchShaderBundleCS::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
	if (FDataDrivenShaderPlatformInfo::GetRequiresBindfulUtilityShaders(Parameters.Platform))
	{
		OutEnvironment.CompilerFlags.Add(CFLAG_ForceBindful);
	}

	OutEnvironment.CompilerFlags.Add(CFLAG_RootConstants);

	OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEX"), ThreadGroupSizeX);
	OutEnvironment.SetDefine(TEXT("USE_SHADER_ROOT_CONSTANTS"), RHISupportsShaderRootConstants(Parameters.Platform) ? 1 : 0);
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
}
