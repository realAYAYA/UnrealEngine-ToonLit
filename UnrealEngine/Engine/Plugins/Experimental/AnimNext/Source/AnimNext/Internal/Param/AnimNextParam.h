// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Param/ParamType.h"
#include "AnimNextParam.generated.h"

USTRUCT()
struct FAnimNextParam
{
	GENERATED_BODY()

	FAnimNextParam() = default;

	FAnimNextParam(FName InName, const FAnimNextParamType& InType)
		: Name(InName)
		, Type(InType)
	{}

	UPROPERTY(EditAnywhere, Category = "Parameter")
	FName Name;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	FAnimNextParamType Type;
};