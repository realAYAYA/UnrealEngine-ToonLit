// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ShaderParameterMetadata.h"

struct FShaderCompilerInput;
struct FShaderCompilerOutput;

enum class EShaderParameterType : uint8;

enum class EBindlessConversionType : uint8
{
	None,
	Resource,
	Sampler
};

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
		FStringView ParsedName; /** View into FShaderParameterParser::OriginalParsedShader */
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

		int32 ParsedPragmaLineoffset = 0;
		int32 ParsedLineOffset = 0;

		/** Character position of the start and end of the parameter decelaration in FParsedShaderParameter::OriginalParsedShader */
		int32 ParsedCharOffsetStart = INDEX_NONE;
		int32 ParsedCharOffsetEnd = INDEX_NONE;

		EBindlessConversionType BindlessConversionType{};
		EShaderParameterType ConstantBufferParameterType{};

		bool bGloballyCoherent = false;

		friend class FShaderParameterParser;
	};

	FShaderParameterParser();
	FShaderParameterParser(const TCHAR* InConstantBufferType);
	FShaderParameterParser(const TCHAR* InConstantBufferType, TArrayView<const TCHAR* const> InExtraSRVTypes, TArrayView<const TCHAR* const> InExtraUAVTypes);
	virtual ~FShaderParameterParser();

	/** Parses the preprocessed shader code and applies the necessary modifications to it. */
	bool ParseAndModify(
		const FShaderCompilerInput& CompilerInput,
		FShaderCompilerOutput& CompilerOutput,
		FString& PreprocessedShaderSource
	);

	UE_DEPRECATED(5.2, "ParseAndModify doesn't need ConstantBufferType anymore")
	bool ParseAndModify(
		const FShaderCompilerInput& CompilerInput,
		FShaderCompilerOutput& CompilerOutput,
		FString& PreprocessedShaderSource,
		const TCHAR* InConstantBufferType
	)
	{
		return ParseAndModify(CompilerInput, CompilerOutput, PreprocessedShaderSource);
	}

	/** Parses the preprocessed shader code and move the parameters into root constant buffer */
	UE_DEPRECATED(5.1, "ParseAndModify should be called instead.")
	bool ParseAndMoveShaderParametersToRootConstantBuffer(
		const FShaderCompilerInput& CompilerInput,
		FShaderCompilerOutput& CompilerOutput,
		FString& PreprocessedShaderSource,
		const TCHAR* InConstantBufferType)
	{
		return ParseAndModify(CompilerInput, CompilerOutput, PreprocessedShaderSource);
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

protected:
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
		FString& PreprocessedShaderSource
	);

	void ExtractFileAndLine(int32 PragamLineoffset, int32 LineOffset, FString& OutFile, FString& OutLine) const;

	/**
	* Generates shader source code to declare a bindless resource or sampler (for automatic bindless conversion).
	* May be overriden to allow custom implementations for different platforms.
	*/
	virtual FString GenerateBindlessParameterDeclaration(const FParsedShaderParameter& ParsedParameter) const;

	const TCHAR* const ConstantBufferType = nullptr;

	const TArray<const TCHAR*> ExtraSRVTypes;
	const TArray<const TCHAR*> ExtraUAVTypes;

	FString OriginalParsedShader;

	TMap<FString, FParsedShaderParameter> ParsedParameters;

	bool bBindlessResources = false;
	bool bBindlessSamplers = false;

	/** Indicates that parameters should be moved to the root cosntant buffer. */
	bool bNeedToMoveToRootConstantBuffer = false;

	/** Indicates that parameters were actually moved to the root constant buffer. */
	bool bMovedLoosedParametersToRootConstantBuffer = false;
};
