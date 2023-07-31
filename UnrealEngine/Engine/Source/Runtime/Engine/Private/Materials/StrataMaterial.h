// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StrataDefinitions.h"
#include "Containers/Map.h"
#include "MaterialCompiler.h"


class FMaterialCompiler;

FString GetStrataBSDFName(uint8 BSDFType);

FStrataRegisteredSharedLocalBasis StrataCompilationInfoCreateNullSharedLocalBasis();
FStrataRegisteredSharedLocalBasis StrataCompilationInfoCreateSharedLocalBasis(FMaterialCompiler* Compiler, int32 NormalCodeChunk, int32 TangentCodeChunk = INDEX_NONE);

