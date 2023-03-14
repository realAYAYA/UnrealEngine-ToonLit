// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ShaderParameterMetadata.h"

struct FShaderCompilerInput;
struct FShaderCompilerOutput;

enum class EShaderParameterType : uint8;

/** Validates and moves all the shader loose data parameter defined in the root scope of the shader into the root uniform buffer. */
class SHADERCOMPILERCOMMON_API FShaderParameterParser
{
public:
	struct FParsedShaderParameter
	{
	public:
		/** Original information about the member. */
		const FShaderParametersMetadata::FMember* Member = nullptr;

		/** Information found about the member when parsing the preprocessed code. */
		FStringView ParsedType; /** View into FShaderParameterParser::OriginalParsedShader */
		FStringView ParsedArraySize; /** View into FShaderParameterParser::OriginalParsedShader */

		/** Offset the member should be in the constant buffer. */
		int32 ConstantBufferOffset = 0;

		/* Returns whether the shader parameter has been found when parsing. */
		bool IsFound() const
		{
			return !ParsedType.IsEmpty();
		}

		/** Returns whether the shader parameter is bindable to the shader parameter structure. */
		bool IsBindable() const
		{
			return Member != nullptr;
		}

	private:
		int32 ParsedPragmaLineoffset = 0;
		int32 ParsedLineOffset = 0;

		/** Character position of the start and end of the parameter decelaration in FParsedShaderParameter::OriginalParsedShader */
		int32 ParsedCharOffsetStart = 0;
		int32 ParsedCharOffsetEnd = 0;

		EShaderParameterType BindlessConversionType{};
		EShaderParameterType ConstantBufferParameterType{};

		friend class FShaderParameterParser;
	};

	FShaderParameterParser();
	FShaderParameterParser(TArrayView<const TCHAR*> InExtraSRVTypes, TArrayView<const TCHAR*> InExtraUAVTypes);
	~FShaderParameterParser();

	/** Parses the preprocessed shader code and applies the necessary modifications to it. */
	bool ParseAndModify(
		const FShaderCompilerInput& CompilerInput,
		FShaderCompilerOutput& CompilerOutput,
		FString& PreprocessedShaderSource,
		const TCHAR* ConstantBufferType
	);

	/** Parses the preprocessed shader code and move the parameters into root constant buffer */
	UE_DEPRECATED(5.1, "ParseAndModify should be called instead.")
	bool ParseAndMoveShaderParametersToRootConstantBuffer(
		const FShaderCompilerInput& CompilerInput,
		FShaderCompilerOutput& CompilerOutput,
		FString& PreprocessedShaderSource,
		const TCHAR* ConstantBufferType)
	{
		return ParseAndModify(CompilerInput, CompilerOutput, PreprocessedShaderSource, ConstantBufferType);
	}

	/** Gets parsing information from a parameter binding name. */
	const FParsedShaderParameter& FindParameterInfos(const FString& ParameterName) const
	{
		return ParsedParameters.FindChecked(ParameterName);
	}

	/** Validates the shader parameter in code is compatible with the shader parameter structure. */
	void ValidateShaderParameterType(
		const FShaderCompilerInput& CompilerInput,
		const FString& ShaderBindingName,
		int32 ReflectionOffset,
		int32 ReflectionSize,
		bool bPlatformSupportsPrecisionModifier,
		FShaderCompilerOutput& CompilerOutput) const;

	void ValidateShaderParameterType(
		const FShaderCompilerInput& CompilerInput,
		const FString& ShaderBindingName,
		int32 ReflectionOffset,
		int32 ReflectionSize,
		FShaderCompilerOutput& CompilerOutput) const
	{
		ValidateShaderParameterType(CompilerInput, ShaderBindingName, ReflectionOffset, ReflectionSize, false, CompilerOutput);
	}

	/** Validates shader parameter map is compatible with the shader parameter structure. */
	void ValidateShaderParameterTypes(
		const FShaderCompilerInput& CompilerInput,
		bool bPlatformSupportsPrecisionModifier,
		FShaderCompilerOutput& CompilerOutput) const;

	void ValidateShaderParameterTypes(
		const FShaderCompilerInput& CompilerInput,
		FShaderCompilerOutput& CompilerOutput) const
	{
		ValidateShaderParameterTypes(CompilerInput, false, CompilerOutput);
	}

	/** Gets file and line of the parameter in the shader source code. */
	void GetParameterFileAndLine(const FParsedShaderParameter& ParsedParameter, FString& OutFile, FString& OutLine) const
	{
		return ExtractFileAndLine(ParsedParameter.ParsedPragmaLineoffset, ParsedParameter.ParsedLineOffset, OutFile, OutLine);
	}

private:
	/** Parses the preprocessed shader code */
	bool ParseParameters(
		const FShaderCompilerInput& CompilerInput,
		FShaderCompilerOutput& CompilerOutput
	);

	void RemoveMovingParametersFromSource(
		FString& PreprocessedShaderSource
	);

	/** Converts parsed parameters into their bindless forms. */
	void ApplyBindlessModifications(
		FString& PreprocessedShaderSource
	);

	/** Moves parsed parameters into the root constant buffer. */
	bool MoveShaderParametersToRootConstantBuffer(
		const FShaderCompilerInput& CompilerInput,
		FShaderCompilerOutput& CompilerOutput,
		FString& PreprocessedShaderSource,
		const TCHAR* ConstantBufferType
	);

	void ExtractFileAndLine(int32 PragamLineoffset, int32 LineOffset, FString& OutFile, FString& OutLine) const;

	TArray<const TCHAR*> ExtraSRVTypes;
	TArray<const TCHAR*> ExtraUAVTypes;

	FString OriginalParsedShader;

	TMap<FString, FParsedShaderParameter> ParsedParameters;

	bool bHasRootParameters = false;
	bool bBindlessResources = false;
	bool bBindlessSamplers = false;

	/** Indicates that parameters should be moved to the root cosntant buffer. */
	bool bNeedToMoveToRootConstantBuffer = false;

	/** Indicates that parameters were actually moved to the root constant buffer. */
	bool bMovedLoosedParametersToRootConstantBuffer = false;
};
