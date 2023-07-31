// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#if WITH_EDITOR

#include "CoreMinimal.h"
#include "Templates/RefCounting.h"

class FMaterial;
class FMaterialCompilationOutput;
struct FSharedShaderCompilerEnvironment;
struct FMaterialCompileTargetParameters;
struct FStaticParameterSet;

bool MaterialEmitHLSL(const FMaterialCompileTargetParameters& InCompilerTarget,
	const FStaticParameterSet& InStaticParameters,
	FMaterial& InOutMaterial,
	FMaterialCompilationOutput& OutCompilationOutput,
	TRefCountPtr<FSharedShaderCompilerEnvironment>& OutMaterialEnvironment);

#endif // WITH_EDITOR
