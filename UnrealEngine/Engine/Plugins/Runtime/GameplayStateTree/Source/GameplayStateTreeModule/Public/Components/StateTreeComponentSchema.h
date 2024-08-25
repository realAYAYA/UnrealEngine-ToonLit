// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StateTreeSchema.h"
#include "GameFramework/Actor.h"
#include "StateTreeExecutionTypes.h"
#include "StateTreeComponentSchema.generated.h"

class UBrainComponent;
class UStateTree;

struct FStateTreeExecutionContext;

/**
 * StateTree for Actors with StateTree component. 
 */
UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "StateTree Component", CommonSchema))
class GAMEPLAYSTATETREEMODULE_API UStateTreeComponentSchema : public UStateTreeSchema
{
	GENERATED_BODY()

public:
	UStateTreeComponentSchema();

	UClass* GetContextActorClass() const { return ContextActorClass; };
	
	static bool SetContextRequirements(UBrainComponent& BrainComponent, FStateTreeExecutionContext& Context, bool bLogErrors = false);
	static bool CollectExternalData(const FStateTreeExecutionContext& Context, const UStateTree* StateTree, TArrayView<const FStateTreeExternalDataDesc> Descs, TArrayView<FStateTreeDataView> OutDataViews);

protected:
	virtual bool IsStructAllowed(const UScriptStruct* InScriptStruct) const override;
	virtual bool IsClassAllowed(const UClass* InScriptStruct) const override;
	virtual bool IsExternalItemAllowed(const UStruct& InStruct) const override;
	
	virtual TConstArrayView<FStateTreeExternalDataDesc> GetContextDataDescs() const override;

	virtual void PostLoad() override;

#if WITH_EDITOR
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
	
	const FStateTreeExternalDataDesc& GetContextActorDataDesc() const { return ContextDataDescs[0]; }
	FStateTreeExternalDataDesc& GetContextActorDataDesc() { return ContextDataDescs[0]; }

	/** Actor class the StateTree is expected to run on. Allows to bind to specific Actor class' properties. */
	UPROPERTY(EditAnywhere, Category="Defaults", NoClear)
	TSubclassOf<AActor> ContextActorClass;
	
	UE_DEPRECATED(5.4, "ContextActorDataDesc is being replaced with ContextDataDescs. Call GetContextActorDataDesc to access the equivalent.")
	UPROPERTY()
	FStateTreeExternalDataDesc ContextActorDataDesc;

	UPROPERTY()
	TArray<FStateTreeExternalDataDesc> ContextDataDescs;
};
