// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DMMaterialStage.h"
#include "DMMaterialSubStage.generated.h"

/**
 * A stage that is a subobject of another stage.
 */
UCLASS(BlueprintType, ClassGroup = "Material Designer", meta = (DisplayName = "Material Designer Sub Stage"))
class DYNAMICMATERIALEDITOR_API UDMMaterialSubStage : public UDMMaterialStage
{
	GENERATED_BODY()

	friend class UDMMaterialStageInputThroughput;

public:
	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	static UDMMaterialSubStage* CreateMaterialSubStage(UDMMaterialStage* InParentStage);

	UDMMaterialStage* GetParentStage() const;
	UDMMaterialStage* GetParentMostStage() const;

	//~ Begin UDMMaterialStage
	virtual bool IsCompatibleWithPreviousStage(const UDMMaterialStage* PreviousStage) const override;
	virtual bool IsCompatibleWithNextStage(const UDMMaterialStage* NextStage) const override;
	virtual bool IsRootStage() const override;
	//~ End UDMMaterialStage

	//~ Begin UDMMaterialComponent
	virtual FString GetComponentPathComponent() const override;
	virtual UDMMaterialComponent* GetParentComponent() const override;
	virtual void PostEditorDuplicate(UDynamicMaterialModel* InMaterialModel, UDMMaterialComponent* InParent) override;
	//~ End UDMMaterialComponent

	void SetParentComponent(UDMMaterialComponent* InParentComponent);

protected:
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Material Designer")
	TObjectPtr<UDMMaterialStage> ParentStage;

	UPROPERTY()
	TObjectPtr<UDMMaterialComponent> ParentComponent;

	UDMMaterialSubStage()
		: ParentStage(nullptr)
		, ParentComponent(nullptr)
	{
	}

	//~ Begin UDMMaterialStage
	// Sub stages don't need these bound.
	virtual void AddDelegates() override {}
	virtual void RemoveDelegates() override {}
	//~ End UDMMaterialStage
};
