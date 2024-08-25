// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Windows/WindowsHWrapper.h"
#include "ShaderCompilerCommon.h"

struct FShaderTarget;

enum class ED3DShaderModel
{
	Invalid,
	SM5_0,
	SM6_0,
	SM6_6,
};

inline bool DoesShaderModelRequireDXC(ED3DShaderModel ShaderModel)
{
	return ShaderModel >= ED3DShaderModel::SM6_0;
}

bool PreprocessD3DShader(
	const FShaderCompilerInput& Input,
	const FShaderCompilerEnvironment& MergedEnvironment,
	FShaderPreprocessOutput& PreprocessOutput);

void CompileD3DShader(
	const FShaderCompilerInput& Input,
	const FShaderPreprocessOutput& InPreprocessOutput,
	FShaderCompilerOutput& Output,
	const FString& WorkingDirectory,
	ED3DShaderModel ShaderModel);

/**
 * @param bSecondPassAferUnusedInputRemoval whether we're compiling the shader second time, after having removed the unused inputs discovered in the first pass
 */
bool CompileAndProcessD3DShaderFXC(
	const FShaderCompilerInput& Input,
	const FString& InPreprocessedSource,
	const FString& InEntryPointName,
	const FShaderParameterParser& ShaderParameterParser,
	const TCHAR* ShaderProfile,
	bool bSecondPassAferUnusedInputRemoval,
	FShaderCompilerOutput& Output);

bool CompileAndProcessD3DShaderDXC(
	const FShaderCompilerInput& Input,
	const FString& InPreprocessedSource,
	const FString& InEntryPointName,
	const FShaderParameterParser& ShaderParameterParser,
	const TCHAR* ShaderProfile,
	ED3DShaderModel ShaderModel,
	bool bProcessingSecondTime,
	FShaderCompilerOutput& Output);

bool ValidateResourceCounts(uint32 NumSRVs, uint32 NumSamplers, uint32 NumUAVs, uint32 NumCBs, TArray<FString>& OutFilteredErrors);

struct FD3DSM6ShaderDebugData
{
	FString Name;
	FString DebugInfo;
	TArray<uint8> Contents;

	inline friend FArchive& operator<<(FArchive& Ar, FD3DSM6ShaderDebugData& DebugData)
	{
		Ar << DebugData.Name;
		Ar << DebugData.DebugInfo;
		Ar << DebugData.Contents;
		return Ar;
	}

	inline TConstArrayView<uint8> GetContents() const
	{
		return TConstArrayView<uint8>(Contents);
	}

	inline FString GetFilename() const
	{
		return Name;
	}

	inline FString GetDebugInfo() const
	{
		return DebugInfo;
	}

	TConstArrayView<FD3DSM6ShaderDebugData> GetAllSymbolData() const
	{
		return TConstArrayView<FD3DSM6ShaderDebugData>(this, 1);
	}
};