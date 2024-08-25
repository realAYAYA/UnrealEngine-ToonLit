// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ShaderCompilerCommon.h"
#include "MetalShaderCompiler.h"
#include "HlslccHeaderWriter.h"
#include "HlslccDefinitions.h"

class FMetalCompileShaderSPIRV
{
public:
	static void DoCompileMetalShader(
									 const FShaderCompilerInput& Input,
									 FShaderCompilerOutput& Output,
									 const FString& InPreprocessedShader,
									 FSHAHash GUIDHash,
									 uint32 VersionEnum,
									 EMetalGPUSemantics Semantics,
									 uint32 MaxUnrollLoops,
									 EShaderFrequency Frequency,
									 bool bDumpDebugInfo,
									 const FString& Standard,
									 const FString& MinOSVersion);
};
