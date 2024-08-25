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

enum class 
	UE_DEPRECATED(5.4, "EDumpShaderDefines is no longer used. Shader defines will always be included in preprocessed source debug dumps, but no longer explicitly added to the source by preprocessing.")
	EDumpShaderDefines : uint8
{
	/** Will not be dumped unless Input.DumpDebugInfoPath is set */
	DontCare,
	/** No defines */
	DontIncludeDefines,
	/** Defines will be added in the comments */
	AlwaysIncludeDefines
};

UE_DEPRECATED(5.4, "Please use overload of PreprocessShader accepting a FShaderPreprocessOutput struct.")
extern SHADERPREPROCESSOR_API bool PreprocessShader(
	FString& OutPreprocessedShader,
	FShaderCompilerOutput& ShaderOutput,
	const FShaderCompilerInput& ShaderInput,
	PRAGMA_DISABLE_DEPRECATION_WARNINGS		// FShaderCompilerDefinitions will be made internal in the future, marked deprecated until then
	const FShaderCompilerDefinitions& AdditionalDefines,
	EDumpShaderDefines DefinesPolicy = EDumpShaderDefines::DontCare
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	);

/**
 * Preprocess a shader.
 * @param Output - Preprocess output struct. Source, directives and possibly errors will be populated.
 * @param Input - The shader compiler input.
 * @param MergedEnvironment - The result of merging the Environment and SharedEnvironment from the FShaderCompilerInput
 * (it is assumed this overload is called outside of the worker process which merges this in-place, so this merge step must be
 * performed by the caller)
 * @param AdditionalDefines - Additional defines with which to preprocess the shader.
 * @returns true if the shader is preprocessed without error.
 */
extern SHADERPREPROCESSOR_API bool PreprocessShader(
	FShaderPreprocessOutput& Output,
	const FShaderCompilerInput& Input,
	const FShaderCompilerEnvironment& MergedEnvironment,
	PRAGMA_DISABLE_DEPRECATION_WARNINGS		// FShaderCompilerDefinitions will be made internal in the future, marked deprecated until then
	const FShaderCompilerDefinitions& AdditionalDefines = FShaderCompilerDefinitions()
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
);

UE_DEPRECATED(5.4, "EDumpShaderDefines is no longer used. Shader defines will always be included in preprocessed source debug dumps, but no longer explicitly added to the source by preprocessing.")
inline bool PreprocessShader(
	FShaderPreprocessOutput& Output,
	const FShaderCompilerInput& Input,
	const FShaderCompilerEnvironment& MergedEnvironment,
	PRAGMA_DISABLE_DEPRECATION_WARNINGS		// FShaderCompilerDefinitions will be made internal in the future, marked deprecated until then
	const FShaderCompilerDefinitions& AdditionalDefines,
	EDumpShaderDefines DefinesPolicy
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
)
{
	return PreprocessShader(Output, Input, MergedEnvironment, AdditionalDefines);
}

