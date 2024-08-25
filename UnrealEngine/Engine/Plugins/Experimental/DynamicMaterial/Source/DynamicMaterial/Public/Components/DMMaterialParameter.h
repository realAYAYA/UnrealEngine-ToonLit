// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/DMMaterialLinkedComponent.h"
#include "DMMaterialParameter.generated.h"

class UDynamicMaterialModel;

/**
 * A parameter on a Material Designer Instance.
 */
UCLASS(BlueprintType, meta = (DisplayName = "Material Designer Parameter"))
class DYNAMICMATERIAL_API UDMMaterialParameter : public UDMMaterialLinkedComponent
{
	GENERATED_BODY()

public:
	friend class UDynamicMaterialModel;

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	UDynamicMaterialModel* GetMaterialModel() const;

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	FName GetParameterName() const { return ParameterName; }

#if WITH_EDITOR
	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void RenameParameter(FName InBaseParameterName);

	//~ Begin UObject
	virtual void BeginDestroy() override;
	//~ End UObject

	//~ Begin UDMMaterialComponent
	virtual void PostEditorDuplicate(UDynamicMaterialModel* InMaterialModel, UDMMaterialComponent* InParent) override;
	//~ End UDMMaterialComponent
#endif

protected:
	UPROPERTY(BlueprintReadOnly, VisibleInstanceOnly, Category = "Material Designer")
	FName ParameterName;

private:
	UDMMaterialParameter();

	#if WITH_EDITOR
	//~ Begin UDMMaterialComponent
	virtual void OnComponentRemoved() override;
	//~ End UDMMaterialComponent
#endif
};
