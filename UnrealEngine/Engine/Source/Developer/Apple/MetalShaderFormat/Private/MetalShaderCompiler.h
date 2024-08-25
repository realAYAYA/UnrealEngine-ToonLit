// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Misc/SecureHash.h"
#include "ShaderCompilerCommon.h"
#include "HlslccDefinitions.h"
#include "MetalBackend.h"

#include <string>

struct FMetalShaderDebugInfoJob
{
	FName ShaderFormat;
	FSHAHash Hash;
	FString CompilerVersion;
	FString MinOSVersion;
	FString DebugInfo;
	FString MathMode;
	FString Standard;
	uint32 SourceCRCLen;
	uint32 SourceCRC;
	
	FString MetalCode;
};

struct FMetalShaderDebugInfo
{
	uint32 UncompressedSize;
	TArray<uint8> CompressedData;
	
	friend FArchive& operator<<( FArchive& Ar, FMetalShaderDebugInfo& Info )
	{
		Ar << Info.UncompressedSize << Info.CompressedData;
		return Ar;
	}
};

struct FMetalShaderBytecodeJob
{
	FName ShaderFormat;
	FSHAHash Hash;
    FString Defines;
	FString TmpFolder;
	FString InputFile;
	FString InputPCHFile;
	FString OutputFile;
	FString OutputObjectFile;
	FString CompilerVersion;
	FString MinOSVersion;
	FString PreserveInvariance;
	FString DebugInfo;
	FString MathMode;
	FString Standard;
	FString IncludeDir;
	uint32 SourceCRCLen;
	uint32 SourceCRC;
	bool bRetainObjectFile;
	bool bCompileAsPCH;
	
	FString Message;
	FString Results;
	FString Errors;
	int32 ReturnCode;
};

struct FMetalShaderBytecode
{
	FString NativePath;
	TArray<uint8> OutputFile;
	TArray<uint8> ObjectFile;
	
	friend FArchive& operator<<( FArchive& Ar, FMetalShaderBytecode& Info )
	{
		Ar << Info.NativePath << Info.OutputFile << Info.ObjectFile;
		return Ar;
	}
};

struct FMetalShaderPreprocessed
{
	FString NativePath;
	TArray<uint8> OutputFile;
	TArray<uint8> ObjectFile;
	
	friend FArchive& operator<<( FArchive& Ar, FMetalShaderPreprocessed& Info )
	{
		Ar << Info.NativePath << Info.OutputFile << Info.ObjectFile;
		return Ar;
	}
};

struct FMetalShaderOutputMetaData
{
	uint32 InvariantBuffers = 0;
	uint32 TypedBuffers = 0;
	uint32 TypedUAVs = 0;
	uint32 ConstantBuffers = 0;
};

// Replace the special texture "gl_LastFragData" to a native subpass fetch operation. Returns true if the input source has been modified.
extern bool PatchSpecialTextureInHlslSource(std::string& SourceData, uint32* OutSubpassInputsDim, uint32 SubpassInputDimCount);
