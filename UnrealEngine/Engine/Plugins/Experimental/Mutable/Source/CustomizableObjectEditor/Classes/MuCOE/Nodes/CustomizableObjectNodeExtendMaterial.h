// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuCOE/Nodes/CustomizableObjectNodeMaterialBase.h"
#include "MuCOE/Nodes/CustomizableObjectNodeParentedMaterial.h"
#include "MuCOE/Nodes/CustomizableObjectNodeUseMaterial.h"

#include "CustomizableObjectNodeExtendMaterial.generated.h"

namespace ENodeTitleType { enum Type : int; }

class FArchive;
class UCustomizableObject;
class UCustomizableObjectNode;
class UCustomizableObjectNodeRemapPins;
class UEdGraphPin;
class UObject;
struct FCustomizableObjectNodeExtendMaterialImage;
struct FEdGraphPinReference;


UCLASS()
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodeExtendMaterial :
	public UCustomizableObjectNodeMaterialBase,
	public FCustomizableObjectNodeParentedMaterial,
	public FCustomizableObjectNodeUseMaterial
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = CustomizableObject)
	TArray<FString> Tags;

	// UObject interface.
	virtual void Serialize(FArchive& Ar) override;
	virtual void BeginPostDuplicate(bool bDuplicateForPIE) override;
	
	// UCustomizableObjectNode
	virtual void PostBackwardsCompatibleFixup() override;
	virtual void BackwardsCompatibleFixup() override;
	virtual void UpdateReferencedNodeId(const FGuid& NewGuid) override;

	// Begin EdGraphNode interface
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FText GetTooltipText() const override;

	// UCustomizableObjectNode interface
	virtual void AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins) override;
	virtual void PinConnectionListChanged(UEdGraphPin * Pin) override;
	virtual bool IsNodeOutDatedAndNeedsRefresh() override;
	virtual FString GetRefreshMessage() const override;
	virtual bool IsSingleOutputNode() const override;
	virtual bool CustomRemovePin(UEdGraphPin& Pin) override;

	// UCustomizableObjectNodeMaterialBase interface
	virtual UEdGraphPin* OutputPin() const override;
	virtual void SetParentNode(UCustomizableObject* Object, FGuid NodeId) override;
	TArray<UCustomizableObjectLayout*> GetLayouts() override;
	
	// FCustomizableObjectNodeParentMaterial interface
	virtual void SaveParentNode(UCustomizableObject* Object, FGuid NodeId) override;
	virtual UCustomizableObjectNode& GetNode() override;
	virtual FGuid GetParentNodeId() const override;
	virtual UCustomizableObject* GetParentObject() const override;
	
	// FCustomizableObjectNodeUseMaterial interface
	virtual FCustomizableObjectNodeParentedMaterial& GetNodeParentedMaterial() override;
	virtual TMap<FGuid, FEdGraphPinReference>& GetPinsParameter() override;
	
	// Own interface
	UEdGraphPin* AddMeshPin() const;
	
private:
	UPROPERTY(EditAnywhere, Category = CustomizableObject)
	TObjectPtr<UCustomizableObject> ParentMaterialObject = nullptr;

	UPROPERTY()
	FGuid ParentMaterialNodeId;
	
	/** Relates a Parameter id to a Pin. Only used to improve performance. */
   	UPROPERTY()
   	TMap<FGuid, FEdGraphPinReference> PinsParameter;

	// Deprecated properties
	UPROPERTY()
	TArray<FCustomizableObjectNodeExtendMaterialImage> Images_DEPRECATED;
};

