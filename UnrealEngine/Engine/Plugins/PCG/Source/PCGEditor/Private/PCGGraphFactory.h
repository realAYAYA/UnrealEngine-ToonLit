// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Factories/Factory.h"

#include "PCGGraphFactory.generated.h"

class UPCGGraphInterface;

UCLASS(hidecategories=Object)
class UPCGGraphFactory : public UFactory
{
	GENERATED_BODY()

public:
	UPCGGraphFactory(const FObjectInitializer& ObjectInitializer);

	//~UFactory interface
	virtual UObject* FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	virtual bool ShouldShowInNewMenu() const override;
};

UCLASS(hidecategories = Object)
class UPCGGraphInstanceFactory : public UFactory
{
	GENERATED_BODY()

public:
	UPCGGraphInstanceFactory(const FObjectInitializer& ObjectInitializer);

	UPROPERTY()
	TObjectPtr<UPCGGraphInterface> ParentGraph;
	
	//~UFactory interface
	virtual UObject* FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	virtual bool ShouldShowInNewMenu() const override;
};
