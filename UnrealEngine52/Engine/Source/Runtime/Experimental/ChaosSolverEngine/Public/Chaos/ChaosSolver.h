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
UCLASS(customconstructor)
class CHAOSSOLVERENGINE_API UChaosSolver : public UObject
{
	GENERATED_UCLASS_BODY()

public:
	UChaosSolver(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	bool IsVisible() { return true; }
};



