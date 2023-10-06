// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ShaderParameterMetadata.h"

class FShaderCompilerFlags;
struct FShaderCompilerInput;
struct FShaderCompilerOutput;
struct FShaderCompilerEnvironment;
struct FShaderCompilerError;

enum class EShaderParameterType : uint8;

enum class EBindlessConversionType : uint8
{
	None,
	Resource,
	Sampler
};

/** Validates and moves all the shader loose data parameter defined in the root scope of the shader into the root uniform buffer. */
class FShaderParameterParser
{
public:
	struct FParsedShaderParameter
	{
	public:
		/** Original information about the member. */
		TEnumAsByte<EUniformBufferBaseType> BaseType{ UBMT_INVALID };
		TEnumAsByte<EShaderPrecisionModifier::Type> PrecisionModifier{ EShaderPrecisionModifier::Invalid };
		uint32 NumRows = 0u;
		uint32 NumColumns = 0u;
		uint32 MemberSize = 0u;

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

		int32 ParsedPragmaLineOffset = 0;
		int32 ParsedLineOffset = 0;

		/** Character position of the start and end of the parameter decelaration in FParsedShaderParameter::OriginalParsedShader */
		int32 ParsedCharOffsetStart = INDEX_NONE;
		int32 ParsedCharOffsetEnd = INDEX_NONE;

		EBindlessConversionType BindlessConversionType{};
		EShaderParameterType ConstantBufferParameterType{};

		bool bGloballyCoherent = false;
		bool bIsBindable = false;

		friend class FShaderParameterParser;
	};

	RENDERCORE_API FShaderParameterParser();

	UE_DEPRECATED(5.3, "Use FShaderParameterParser constructor which accepts FShaderCompilerFlags")
	RENDERCORE_API FShaderParameterParser(const TCHAR* InConstantBufferType);
	UE_DEPRECATED(5.3, "Use FShaderParameterParser constructor which accepts FShaderCompilerFlags")
	RENDERCORE_API FShaderParameterParser(const TCHAR* InConstantBufferType, TConstArrayView<const TCHAR*> InExtraSRVTypes, TConstArrayView<const TCHAR*> InExtraUAVTypes);

	RENDERCORE_API FShaderParameterParser(
		FShaderCompilerFlags CompilerFlags,
		const TCHAR* InConstantBufferType = nullptr,
		TConstArrayView<const TCHAR*> InExtraSRVTypes = {},
		TConstArrayView<const TCHAR*> InExtraUAVTypes = {});

	RENDERCORE_API virtual ~FShaderParameterParser();

	FShaderParameterParser& operator=(FShaderParameterParser&&) = default;

	static constexpr const TCHAR* kBindlessResourcePrefix = TEXT("BindlessResource_");
	static constexpr const TCHAR* kBindlessSamplerPrefix = TEXT("BindlessSampler_");

	static RENDERCORE_API EShaderParameterType ParseParameterType(FStringView InType, TConstArrayView<const TCHAR*> InExtraSRVTypes, TConstArrayView<const TCHAR*> InExtraUAVTypes);
	static RENDERCORE_API EShaderParameterType ParseAndRemoveBindlessParameterPrefix(FStringView& InName);
	static RENDERCORE_API EShaderParameterType ParseAndRemoveBindlessParameterPrefix(FString& InName);
	static RENDERCORE_API bool RemoveBindlessParameterPrefix(FString& InName);

	/** Parses the preprocessed shader code and applies the necessary modifications to it. */
	UE_DEPRECATED(5.3, "Use ParseAndModify overload accepting array of FShaderCompilerError instead of passing FShaderCompilerOutput")
	RENDERCORE_API bool ParseAndModify(
		const FShaderCompilerInput& CompilerInput,
		FShaderCompilerOutput& CompilerOutput,
		FString& PreprocessedShaderSource
	);

	RENDERCORE_API bool ParseAndModify(
		const FShaderCompilerInput& CompilerInput,
		TArray<FShaderCompilerError>& OutErrors,
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
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		return ParseAndModify(CompilerInput, CompilerOutput, PreprocessedShaderSource);
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}

	/** Gets parsing information from a parameter binding name. */
	const FParsedShaderParameter& FindParameterInfos(const FString& ParameterName) const
	{
		return ParsedParameters.FindChecked(ParameterName);
	}

	/** Validates the shader parameter in code is compatible with the shader parameter structure. */
	RENDERCORE_API void ValidateShaderParameterType(
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
	RENDERCORE_API void ValidateShaderParameterTypes(
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
		return ExtractFileAndLine(ParsedParameter.ParsedPragmaLineOffset, ParsedParameter.ParsedLineOffset, OutFile, OutLine);
	}

	friend FArchive& operator<<(FArchive& Ar, FShaderParameterParser& Parser);

protected:
	/** Parses the preprocessed shader code */
	RENDERCORE_API bool ParseParameters(
		const FShaderParametersMetadata* RootParametersStructure,
		TArray<FShaderCompilerError>& OutErrors
	);

	RENDERCORE_API void RemoveMovingParametersFromSource(
		FString& PreprocessedShaderSource
	);

	/** Converts parsed parameters into their bindless forms. */
	RENDERCORE_API void ApplyBindlessModifications(
		FString& PreprocessedShaderSource
	);

	/** Moves parsed parameters into the root constant buffer. */
	RENDERCORE_API bool MoveShaderParametersToRootConstantBuffer(
		const FShaderParametersMetadata* RootParametersStructure,
		FString& PreprocessedShaderSource
	);

	RENDERCORE_API void ExtractFileAndLine(int32 PragamLineoffset, int32 LineOffset, FString& OutFile, FString& OutLine) const;

	/**
	* Generates shader source code to declare a bindless resource or sampler (for automatic bindless conversion).
	* May be overriden to allow custom implementations for different platforms.
	*/
	RENDERCORE_API virtual FString GenerateBindlessParameterDeclaration(const FParsedShaderParameter& ParsedParameter) const;

	const TCHAR* ConstantBufferType = nullptr;

	TConstArrayView<const TCHAR*> ExtraSRVTypes;
	TConstArrayView<const TCHAR*> ExtraUAVTypes;

	FString OriginalParsedShader;

	TMap<FString, FParsedShaderParameter> ParsedParameters;

	bool bBindlessResources = false;
	bool bBindlessSamplers = false;

	/** Indicates that parameters should be moved to the root cosntant buffer. */
	bool bNeedToMoveToRootConstantBuffer = false;

	/** Indicates that parameters were actually moved to the root constant buffer. */
	bool bMovedLoosedParametersToRootConstantBuffer = false;
};
