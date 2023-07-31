// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AIController.h"
#include "GridPathAIController.generated.h"

UCLASS(Experimental)
class AGridPathAIController : public AAIController
{
	GENERATED_BODY()
public:
	AGridPathAIController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};