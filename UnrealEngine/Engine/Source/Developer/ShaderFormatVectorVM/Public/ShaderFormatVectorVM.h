// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "HAL/Platform.h"
#include "Serialization/Archive.h"
#include "VectorVM.h"
#include "VectorVMCommon.h"

struct FShaderCompilerInput;
struct FShaderCompilerOutput;

/** Ordered table of functions actually called by the VM script. */
struct FCalledVMFunction
{
	FString Name;
	TArray<bool> InputParamLocations;
	int32 NumOutputs;
	FCalledVMFunction() :NumOutputs(0) {}
};

/** Data which is generated from the hlsl by the VectorVMBackend and fed back into the */
struct FVectorVMCompilationOutput
{
	FVectorVMCompilationOutput(): MaxTempRegistersUsed(0), NumOps(0) {}

	TArray<uint8> ByteCode;

	int32 MaxTempRegistersUsed;

	TArray<int32> InternalConstantOffsets;
	TArray<uint8> InternalConstantData;
	TArray<EVectorVMBaseTypes> InternalConstantTypes;
	
	TArray<FCalledVMFunction> CalledVMFunctionTable;

	FString AssemblyAsString;
	uint32 NumOps;

	FString Errors;
};

inline FArchive& operator<<(FArchive& Ar, FCalledVMFunction& Function)
{
	Ar << Function.Name;
	Ar << Function.InputParamLocations;
	Ar << Function.NumOutputs;
	return Ar;
}

inline FArchive& operator<<(FArchive& Ar, FVectorVMCompilationOutput& Output)
{
	Ar << Output.ByteCode;
	Ar << Output.MaxTempRegistersUsed;
	Ar << Output.InternalConstantOffsets;
	Ar << Output.InternalConstantData;
	Ar << Output.InternalConstantTypes;
	Ar << Output.CalledVMFunctionTable;
	Ar << Output.AssemblyAsString;
	Ar << Output.NumOps;
	Ar << Output.Errors;
	return Ar;
}

UE_DEPRECATED(5.4, "Direct VectorVM compilation will no longer be exposed outside of the TestCompileVectorVMShader function")
inline bool CompileShader_VectorVM(const FShaderCompilerInput& Input, FShaderCompilerOutput& Output, const FString& WorkingDirectory, uint8 Version) { return false; }

UE_DEPRECATED(5.4, "This overload of CompileShader_VectorVM is superceded by TestCompileShaderVectorVM")
inline bool CompileShader_VectorVM(const FShaderCompilerInput& Input, FShaderCompilerOutput& Output, const FString& WorkingDirectory, uint8 Version, FVectorVMCompilationOutput& VMCompilationOutput, bool bSkipBackendOptimizations = false) { return false; }
