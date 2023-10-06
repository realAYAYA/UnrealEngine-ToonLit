// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuCOE/Nodes/CustomizableObjectNode.h"
#include "MuCOE/Nodes/CustomizableObjectNodeParentedMaterial.h"

#include "CustomizableObjectNodeEditMaterialBase.generated.h"

class UCustomizableObject;
class UEdGraphPin;
class UObject;


UCLASS(Abstract)
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodeEditMaterialBase : public UCustomizableObjectNode, public FCustomizableObjectNodeParentedMaterial
{
	GENERATED_BODY()
	
public:
	// FCustomizableObjectNodeParentedMaterial interface
	virtual void SetParentNode(UCustomizableObject* Object, FGuid NodeId) override;
	virtual void SaveParentNode(UCustomizableObject* Object, FGuid NodeId) override;
	virtual UCustomizableObjectNode& GetNode() override;
	virtual FGuid GetParentNodeId() const override;
	virtual UCustomizableObject* GetParentObject() const override;

	// UCustomizableObjectNodeEditMaterialBase interface
	virtual void SetLayoutIndex(const int32 LayoutIndex);

	// Own interface
	/** Index of the layout to use to patch blocks. */
	UPROPERTY(EditAnywhere, Category = CustomizableObject)
	int ParentLayoutIndex = 0;

	void BeginPostDuplicate(bool bDuplicateForPIE) override;

	void UpdateReferencedNodeId(const FGuid& NewGuid) override;

	virtual UEdGraphPin* OutputPin() const
	{
		return FindPin(TEXT("Material"));
	}

private:
	UPROPERTY(EditAnywhere, Category = CustomizableObject)
	TObjectPtr<UCustomizableObject> ParentMaterialObject = nullptr;

	UPROPERTY()
	FGuid ParentMaterialNodeId;
};

