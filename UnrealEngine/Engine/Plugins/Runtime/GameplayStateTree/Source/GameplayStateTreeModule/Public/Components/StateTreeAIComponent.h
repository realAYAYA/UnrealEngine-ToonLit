// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/StateTreeComponent.h"

#include "StateTreeAIComponent.generated.h"

/**
* State tree component designed to be run on an AIController.
* It uses the StateTreeAIComponentSchema that guarantees access to the AIController.
*/
UCLASS(ClassGroup = AI, meta = (BlueprintSpawnableComponent))
class UStateTreeAIComponent : public UStateTreeComponent
{
	GENERATED_BODY()
public:
	// BEGIN IStateTreeSchemaProvider
	TSubclassOf<UStateTreeSchema> GetSchema() const override;
	// END

	virtual bool SetContextRequirements(FStateTreeExecutionContext& Context, bool bLogErrors = false) override;
};
