// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/StateTreeComponentSchema.h"

#include "StateTreeAIComponentSchema.generated.h"

class AAIController;

/**
* State tree schema to be used with StateTreeAIComponent. 
* It guarantees access to an AIController and the Actor context value can be used to access the controlled pawn.
*/
UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "StateTree AI Component", CommonSchema))
class UStateTreeAIComponentSchema : public UStateTreeComponentSchema
{
	GENERATED_BODY()
public:
	UStateTreeAIComponentSchema(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	
	virtual void PostLoad() override;

	virtual bool IsStructAllowed(const UScriptStruct* InScriptStruct) const override;

	static bool SetContextRequirements(UBrainComponent& BrainComponent, FStateTreeExecutionContext& Context, bool bLogErrors = false);

#if WITH_EDITOR
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR

protected:
	/** AIController class the StateTree is expected to run on. Allows to bind to specific Actor class' properties. */
	UPROPERTY(EditAnywhere, Category = "Defaults", NoClear)
	TSubclassOf<AAIController> AIControllerClass = nullptr;
};
