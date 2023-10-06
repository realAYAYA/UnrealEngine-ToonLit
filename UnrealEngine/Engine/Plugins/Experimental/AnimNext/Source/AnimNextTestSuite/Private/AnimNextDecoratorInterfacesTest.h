// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "DecoratorBase/DecoratorHandle.h"
#include "DecoratorBase/DecoratorSharedData.h"

#include "AnimNextDecoratorInterfacesTest.generated.h"

USTRUCT()
struct FDecoratorWithOneChildSharedData : public FAnimNextDecoratorSharedData
{
	GENERATED_BODY()

	UPROPERTY()
	FAnimNextDecoratorHandle Child;
};

USTRUCT()
struct FDecoratorWithChildrenSharedData : public FAnimNextDecoratorSharedData
{
	GENERATED_BODY()

	UPROPERTY()
	FAnimNextDecoratorHandle Children[2];
};
