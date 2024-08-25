// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"
#include "PhysicsControlProfileAssetFactory.generated.h"

UCLASS()
class UPhysicsControlProfileAssetFactory : public UFactory
{
	GENERATED_BODY()
public:
	UPhysicsControlProfileAssetFactory(const FObjectInitializer& ObjectInitializer);
	UObject* FactoryCreateNew(
		UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn);
};
