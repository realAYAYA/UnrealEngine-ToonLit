// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Factories/Factory.h"
#include "PoseCorrectivesFactory.generated.h"

UCLASS(hidecategories = Object, MinimalAPI)
class UPoseCorrectivesFactory
	: public UFactory
{
	GENERATED_BODY()

public:

	UPoseCorrectivesFactory();

	virtual UObject* FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags InFlags, UObject* InContext, FFeedbackContext* InWarn) override;
};
