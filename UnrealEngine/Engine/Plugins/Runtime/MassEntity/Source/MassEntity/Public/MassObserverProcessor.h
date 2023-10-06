// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassProcessor.h"
#include "MassObserverProcessor.generated.h"


UCLASS(abstract)
class MASSENTITY_API UMassObserverProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UMassObserverProcessor();

protected:
	virtual void PostInitProperties() override;

	/** 
	 * By default registers this class as Operation observer of ObservedType. Override to register for multiple 
	 * operations and/or types 
	 */
	virtual void Register();

protected:
	UPROPERTY(EditDefaultsOnly, Category = Processor, config)
	bool bAutoRegisterWithObserverRegistry = true;

	/** Determines which Fragment or Tag type this given UMassObserverProcessor will be observing */
	UPROPERTY()
	TObjectPtr<UScriptStruct> ObservedType = nullptr;

	EMassObservedOperation Operation = EMassObservedOperation::MAX;
};
