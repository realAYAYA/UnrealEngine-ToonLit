// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Factories/Factory.h"
#include "MoviePipelineConfigFactory.generated.h"

// For backwards compatibility 5.2
UE_DEPRECATED(5.2, "UMoviePipelineMasterConfigFactory is deprecated. Please use UMoviePipelinePrimaryConfigFactory.")
typedef UMoviePipelinePrimaryConfigFactory UMoviePipelineMasterConfigFactory;

// Forward Declare
UCLASS(BlueprintType)
class MOVIERENDERPIPELINEEDITOR_API UMoviePipelinePrimaryConfigFactory : public UFactory
{
    GENERATED_BODY()
public:
	UMoviePipelinePrimaryConfigFactory();
	//~ Begin UFactory Interface
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	virtual uint32 GetMenuCategories() const override;
	//~ End UFactory Interface
};

// Forward Declare
UCLASS(BlueprintType)
class MOVIERENDERPIPELINEEDITOR_API UMoviePipelineShotConfigFactory : public UFactory
{
    GENERATED_BODY()
public:
	UMoviePipelineShotConfigFactory();
	//~ Begin UFactory Interface
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	virtual uint32 GetMenuCategories() const override;
	//~ End UFactory Interface
};