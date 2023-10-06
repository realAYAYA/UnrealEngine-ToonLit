// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Physics/Experimental/PhysScene_Chaos.h"

#include "ChaosSolver.generated.h"

class UChaosSolver;

/**
* UChaosSolver (UObject)
*
*/
UCLASS(customconstructor, MinimalAPI)
class UChaosSolver : public UObject
{
	GENERATED_UCLASS_BODY()

public:
	CHAOSSOLVERENGINE_API UChaosSolver(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	bool IsVisible() { return true; }
};



