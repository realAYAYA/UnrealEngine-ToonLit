// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Windows/WindowsHWrapper.h"
#include "ShaderCompilerCommon.h"

// TODO: Lock D3D12 to SM 6.6 min spec
#define USE_SHADER_MODEL_6_6 1

// Controls whether r.Shaders.RemoveDeadCode should be honored
#ifndef UE_D3D_SHADER_COMPILER_ALLOW_DEAD_CODE_REMOVAL
#define UE_D3D_SHADER_COMPILER_ALLOW_DEAD_CODE_REMOVAL 1
#endif // UE_D3D_SHADER_COMPILER_ALLOW_DEAD_CODE_REMOVAL

struct FShaderTarget;

enum class ELanguage
{
	SM5,
	SM6,
	ES3_1,
};

void CompileShader_Windows(const struct FShaderCompilerInput& Input,struct FShaderCompilerOutput& Output,const class FString& WorkingDirectory, ELanguage Language);

/**
 * @param bSecondPassAferUnusedInputRemoval whether we're compiling the shader second time, after having removed the unused inputs discovered in the first pass
 */
bool CompileAndProcessD3DShaderFXC(FString& PreprocessedShaderSource,
	uint32 CompileFlags,
	const FShaderCompilerInput& Input,
	const FShaderParameterParser& ShaderParameterParser,
	FString& EntryPointName,
	const TCHAR* ShaderProfile, bool bSecondPassAferUnusedInputRemoval,
	FShaderCompilerOutput& Output);

bool CompileAndProcessD3DShaderDXC(FString& PreprocessedShaderSource,
	uint32 CompileFlags,
	const FShaderCompilerInput& Input,
	const FShaderParameterParser& ShaderParameterParser,
	FString& EntryPointName,
	const TCHAR* ShaderProfile, ELanguage Language, bool bProcessingSecondTime,
	FShaderCompilerOutput& Output);

bool ValidateResourceCounts(uint32 NumSRVs, uint32 NumSamplers, uint32 NumUAVs, uint32 NumCBs, TArray<FString>& OutFilteredErrors);
