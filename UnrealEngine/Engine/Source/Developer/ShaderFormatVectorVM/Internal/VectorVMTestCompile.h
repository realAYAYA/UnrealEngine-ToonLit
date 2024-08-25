// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ShaderFormatVectorVM.h"

extern SHADERFORMATVECTORVM_API bool TestCompileVectorVMShader(const FShaderCompilerInput& Input, const FString& WorkingDirectory, FVectorVMCompilationOutput& VMCompilationOutput, bool bSkipBackendOptimizations);
