// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Factories/Factory.h"
#include "MovieGraphConfigFactory.generated.h"

UCLASS(BlueprintType)
class MOVIERENDERPIPELINEEDITOR_API UMovieGraphConfigFactory : public UFactory
{
    GENERATED_BODY()
public:
	UMovieGraphConfigFactory();
	//~ Begin UFactory Interface
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	virtual bool ShouldShowInNewMenu() const override;
	//~ End UFactory Interface
};