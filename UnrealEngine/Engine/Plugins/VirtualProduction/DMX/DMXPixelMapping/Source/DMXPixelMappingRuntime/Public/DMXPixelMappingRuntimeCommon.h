// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

DECLARE_LOG_CATEGORY_EXTERN(LogDMXPixelMappingRuntime, All, All);

class UDMXPixelMapping;
class UDMXPixelMappingBaseComponent;
class UDMXPixelMappingRootComponent;
class UDMXPixelMappingRendererComponent;
class UDMXPixelMappingOutputComponent;
class UDMXPixelMappingOutputDMXComponent;
class UDMXPixelMappingFixtureGroupComponent;
class UDMXPixelMappingMatrixComponent;
class UDMXPixelMappingMatrixCellComponent;

using TComponentPredicate = TFunctionRef<void(UDMXPixelMappingBaseComponent*)>;
template <typename Type>
using TComponentPredicateType = TFunctionRef<void(Type*)>;
