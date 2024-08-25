// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Class.h"
#include "Param/ParamType.h"
#include "Param/ParamId.h"

namespace UE::AnimNext
{

struct FContext;
struct FParamStack;
struct FParamStackLayer;

// Context providing methods for mutating & interrogating the anim interface runtime
struct ANIMNEXT_API FContext
{
public:
	FContext();
	explicit FContext(float InDeltaTime);

	// Access the parameter stack, cached on construction
	FParamStack& GetMutableParamStack() const { return Stack; }
	const FParamStack& GetParamStack() const { return Stack; }

	float GetDeltaTime() const { return DeltaTime; }

private:
	FContext(const FContext& Other) = delete;
	FContext& operator=(const FContext&) = delete;

	FParamStack& Stack;
	float DeltaTime = 0.0f;
};

}
