// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"

#include "PCGCommon.h"

#include "PCGEditorGraphNodeBase.generated.h"

class UPCGNode;
class UPCGPin;
class UToolMenu;

UCLASS()
class UPCGEditorGraphNodeBase : public UEdGraphNode
{
	GENERATED_BODY()

public:
	void Construct(UPCGNode* InPCGNode);

	// ~Begin UObject interface
	virtual void BeginDestroy() override;
	virtual void PostTransacted(const FTransactionObjectEvent& TransactionEvent) override;
	// ~End UObject interface

	// ~Begin UEdGraphNode interface
	virtual void GetNodeContextMenuActions(UToolMenu* Menu, class UGraphNodeContextMenuContext* Context) const override;
	virtual void AutowireNewNode(UEdGraphPin* FromPin) override;
	virtual void PrepareForCopying() override;
	virtual bool CanCreateUnderSpecifiedSchema(const UEdGraphSchema* Schema) const override;
	virtual void ReconstructNode() override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FLinearColor GetNodeBodyTintColor() const override;
	virtual void PostPasteNode() override;
	virtual FText GetTooltipText() const override;
	virtual void GetPinHoverText(const UEdGraphPin& Pin, FString& HoverTextOut) const override;
	virtual UObject* GetJumpTargetForDoubleClick() const override;
	virtual void OnUpdateCommentText(const FString& NewComment);
	virtual void OnCommentBubbleToggled(bool bInCommentBubbleVisible) override;
	// ~End UEdGraphNode interface

	UPCGNode* GetPCGNode() { return PCGNode; }
	const UPCGNode* GetPCGNode() const { return PCGNode; }
	void PostCopy();
	void PostPaste();
	void SetInspected(bool InIsInspecting) { bIsInspected = InIsInspecting; }
	bool GetInspected() const { return bIsInspected; }

	DECLARE_DELEGATE(FOnPCGEditorGraphNodeChanged);
	FOnPCGEditorGraphNodeChanged OnNodeChangedDelegate;

protected:
	static FEdGraphPinType GetPinType(const UPCGPin* InPin);

	void RebuildEdgesFromPins();

	void OnNodeChanged(UPCGNode* InNode, EPCGChangeType ChangeType);
	void OnPickColor();
	void OnColorPicked(FLinearColor NewColor);
	void UpdateCommentBubblePinned();
	void UpdatePosition();

	UPROPERTY()
	TObjectPtr<UPCGNode> PCGNode = nullptr;

	bool bDisableReconstructFromNode = false;
	bool bIsInspected = false;
};
