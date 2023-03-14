// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/Platform.h"
#include "ShaderCore.h"

class FShaderCompilerDefinitions;
class FString;
struct FShaderCompilerInput;
struct FShaderCompilerOutput;

/** Governs the behavior for adding shader defines to the preprocessed source. Can be helpful for the debugging, but makes the source unique
    which can prevent efficient caching.
  */
enum class EDumpShaderDefines : uint8
{
	/** Will not be dumped unless Input.DumpDebugInfoPath is set */
	DontCare,
	/** No defines */
	DontIncludeDefines,
	/** Defines will be added in the comments */
	AlwaysIncludeDefines
};

/**
 * Preprocess a shader.
 * @param OutPreprocessedShader - Upon return contains the preprocessed source code.
 * @param ShaderOutput - ShaderOutput to which errors can be added.
 * @param ShaderInput - The shader compiler input.
 * @param AdditionalDefines - Additional defines with which to preprocess the shader.
 * @param bShaderDumpDefinesAsCommentedCode - Whether to add shader definitions as comments.
 * @returns true if the shader is preprocessed without error.
 */
extern SHADERPREPROCESSOR_API bool PreprocessShader(
	FString& OutPreprocessedShader,
	FShaderCompilerOutput& ShaderOutput,
	const FShaderCompilerInput& ShaderInput,
	const FShaderCompilerDefinitions& AdditionalDefines,
	EDumpShaderDefines DefinesPolicy = EDumpShaderDefines::DontCare);
