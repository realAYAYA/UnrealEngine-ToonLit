// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DMMaterialComponent.h"
#include "DMMaterialLinkedComponent.generated.h"

UCLASS(Abstract, BlueprintType, meta = (DisplayName = "Material Designer Linked Component"))
class DYNAMICMATERIAL_API UDMMaterialLinkedComponent : public UDMMaterialComponent
{
	GENERATED_BODY()

public:
	UDMMaterialLinkedComponent() = default;

#if WITH_EDITOR
	virtual UDMMaterialComponent* GetParentComponent() const override;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void SetParentComponent(UDMMaterialComponent* InParentComponent);

	virtual void PostEditorDuplicate(UDynamicMaterialModel* InMaterialModel, UDMMaterialComponent* InParent) override;
#endif

protected:
#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<UDMMaterialComponent> ParentComponent = nullptr;
#endif
};

