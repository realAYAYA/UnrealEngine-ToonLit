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
	Invalid,
};

bool PreprocessD3DShader(
	const struct FShaderCompilerInput& Input,
	const struct FShaderCompilerEnvironment& MergedEnvironment,
	class FShaderPreprocessOutput& PreprocessOutput,
	ELanguage Language);

void CompileD3DShader(
	const struct FShaderCompilerInput& Input,
	const class FShaderPreprocessOutput& PreprocessOutput,
	struct FShaderCompilerOutput& Output,
	const class FString& WorkingDirectory,
	ELanguage Language);

/**
 * @param bSecondPassAferUnusedInputRemoval whether we're compiling the shader second time, after having removed the unused inputs discovered in the first pass
 */
bool CompileAndProcessD3DShaderFXC(
	const FShaderPreprocessOutput& PreprocessOutput,
	const FShaderCompilerInput& Input,
	const FShaderParameterParser& ShaderParameterParser,
	const TCHAR* ShaderProfile,
	bool bSecondPassAferUnusedInputRemoval,
	FShaderCompilerOutput& Output);

bool CompileAndProcessD3DShaderDXC(
	const FShaderPreprocessOutput& PreprocessOutput,
	const FShaderCompilerInput& Input,
	const FShaderParameterParser& ShaderParameterParser,
	const TCHAR* ShaderProfile,
	ELanguage Language,
	bool bProcessingSecondTime,
	FShaderCompilerOutput& Output);

bool ValidateResourceCounts(uint32 NumSRVs, uint32 NumSamplers, uint32 NumUAVs, uint32 NumCBs, TArray<FString>& OutFilteredErrors);
