// Copyright Epic Games, Inc. All Rights Reserved.

/** Factory which allows import of an ChaosSolverAsset */

#pragma once

#include "Factories/Factory.h"

#include "ChaosSolverFactory.generated.h"

class UChaosSolver;


/**
* Factory for Simple Cube
*/

UCLASS()
class CHAOSSOLVEREDITOR_API UChaosSolverFactory : public UFactory
{
    GENERATED_UCLASS_BODY()

	//~ Begin UFactory Interface
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	static UChaosSolver* StaticFactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn);
};



#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "Chaos/ChaosSolver.h"
#include "CoreMinimal.h"
#endif
