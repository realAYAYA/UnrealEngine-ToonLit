// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/Platform.h"
#include "ShaderCore.h"

class FShaderCompilerDefinitions;
class FShaderPreprocessOutput;
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

/**
 * Preprocess a shader.
 * @param Output - Preprocess output struct. Source, directives and possibly errors will be populated.
 * @param Input - The shader compiler input.
 * @param MergedEnvironment - The result of merging the Environment and SharedEnvironment from the FShaderCompilerInput
 * (it is assumed this overload is called outside of the worker process which merges this in-place, so this merge step must be
 * performed by the caller)
 * @param AdditionalDefines - Additional defines with which to preprocess the shader.
 * @param bShaderDumpDefinesAsCommentedCode - Whether to add shader definitions as comments.
 * @returns true if the shader is preprocessed without error.
 */
extern SHADERPREPROCESSOR_API bool PreprocessShader(
	FShaderPreprocessOutput& Output,
	const FShaderCompilerInput& Input,
	const FShaderCompilerEnvironment& MergedEnvironment,
	const FShaderCompilerDefinitions& AdditionalDefines,
	EDumpShaderDefines DefinesPolicy = EDumpShaderDefines::DontCare);
