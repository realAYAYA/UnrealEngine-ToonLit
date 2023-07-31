// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StateTreeSchema.h"
#include "GameFramework/Actor.h"
#include "Templates/SubclassOf.h"
#include "GameplayInteractionStateTreeSchema.generated.h"

struct FStateTreeExternalDataDesc;

UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Gameplay Interactions"))
class GAMEPLAYINTERACTIONSMODULE_API UGameplayInteractionStateTreeSchema : public UStateTreeSchema
{
	GENERATED_BODY()

public:
	UGameplayInteractionStateTreeSchema();

	UClass* GetContextActorClass() const { return ContextActorClass; };
	UClass* GetSmartObjectActorClass() const { return SmartObjectActorClass; };

protected:
	virtual bool IsStructAllowed(const UScriptStruct* InScriptStruct) const override;
	virtual bool IsClassAllowed(const UClass* InScriptStruct) const override;
	virtual bool IsExternalItemAllowed(const UStruct& InStruct) const override;

	virtual TConstArrayView<FStateTreeExternalDataDesc> GetContextDataDescs() const override { return ContextDataDescs; }

	virtual void PostLoad() override;

#if WITH_EDITOR
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
	
	/** Actor class the StateTree is expected to run on. Allows to bind to specific Actor class' properties. */
	UPROPERTY(EditAnywhere, Category="Defaults")
	TSubclassOf<AActor> ContextActorClass;

	/** Actor class of the SmartObject the StateTree is expected to run with. Allows to bind to specific Actor class' properties. */
	UPROPERTY(EditAnywhere, Category="Defaults")
	TSubclassOf<AActor> SmartObjectActorClass;

	/** List of named external data required by schema and provided to the state tree through the execution context. */
	UPROPERTY()
	TArray<FStateTreeExternalDataDesc> ContextDataDescs;
};
