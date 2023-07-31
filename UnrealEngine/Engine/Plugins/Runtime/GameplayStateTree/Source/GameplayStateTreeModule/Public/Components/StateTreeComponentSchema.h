// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StateTreeSchema.h"
#include "GameFramework/Actor.h"
#include "Templates/SubclassOf.h"
#include "StateTreeComponentSchema.generated.h"

/**
 * StateTree for Actors with StateTree component. 
 */
UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "StateTree Component", CommonSchema))
class UStateTreeComponentSchema : public UStateTreeSchema
{
	GENERATED_BODY()

public:
	UStateTreeComponentSchema();

	UClass* GetContextActorClass() const { return ContextActorClass; };
	
protected:
	virtual bool IsStructAllowed(const UScriptStruct* InScriptStruct) const override;
	virtual bool IsClassAllowed(const UClass* InScriptStruct) const override;
	virtual bool IsExternalItemAllowed(const UStruct& InStruct) const override;
	
	virtual TConstArrayView<FStateTreeExternalDataDesc> GetContextDataDescs() const;

	virtual void PostLoad() override;

#if WITH_EDITOR
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
	
	/** Actor class the StateTree is expected to run on. Allows to bind to specific Actor class' properties. */
	UPROPERTY(EditAnywhere, Category="Defaults")
	TSubclassOf<AActor> ContextActorClass;
	
	UPROPERTY()
	FStateTreeExternalDataDesc ContextActorDataDesc;
};
