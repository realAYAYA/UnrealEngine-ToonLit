// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Shader/PreshaderTypes.h"

class FUniformExpressionSet;
struct FMaterialRenderContext;

namespace UE
{
namespace Shader
{

class FPreshaderData;

struct FPreshaderDataContext
{
	explicit FPreshaderDataContext(const FPreshaderData& InData);
	FPreshaderDataContext(const FPreshaderDataContext& InContext, uint32 InOffset, uint32 InSize);

	const uint8* RESTRICT Ptr;
	const uint8* RESTRICT EndPtr;
	TArrayView<const FScriptName> Names;
	TArrayView<const FPreshaderStructType> StructTypes;
	TArrayView<const EValueComponentType> StructComponentTypes;
};

ENGINE_API FPreshaderValue EvaluatePreshader(const FUniformExpressionSet* UniformExpressionSet, const FMaterialRenderContext& Context, FPreshaderStack& Stack, FPreshaderDataContext& RESTRICT Data);

} // namespace Shader
} // namespace UE
