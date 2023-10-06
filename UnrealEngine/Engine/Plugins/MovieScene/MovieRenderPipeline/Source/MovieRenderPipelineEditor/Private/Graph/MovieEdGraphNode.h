// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreTypes.h"
#include "EdGraph/EdGraphNode.h"
#include "Graph/MovieGraphNode.h"

#include "MovieEdGraphNode.generated.h"

class UMovieGraphNode;

UCLASS(Abstract)
class UMoviePipelineEdGraphNodeBase : public UEdGraphNode
{
	GENERATED_BODY()
public:
	UMovieGraphNode* GetRuntimeNode() const { return RuntimeNode; }

	void Construct(UMovieGraphNode* InRuntimeNode);

	void OnRuntimeNodeChanged(const UMovieGraphNode* InChangedNode);

	//~ Begin UEdGraphNode Interface
	virtual void ReconstructNode() override;
	virtual void AutowireNewNode(UEdGraphPin* FromPin) override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;
	virtual bool ShowPaletteIconOnNode() const override;
	virtual void OnUpdateCommentText(const FString& NewComment) override;
	virtual void OnCommentBubbleToggled(bool bInCommentBubbleVisible) override;
	virtual void PrepareForCopying() override;
	virtual void PostPasteNode() override;
	virtual void GetNodeContextMenuActions(UToolMenu* Menu, UGraphNodeContextMenuContext* Context) const override;
	//~ End UEdGraphNode Interface

	static FEdGraphPinType GetPinType(EMovieGraphValueType ValueType, bool bIsBranch);
	static FEdGraphPinType GetPinType(const UMovieGraphPin* InPin);

	// Called after PrepareForCopying to restore changes made during preparation. 
	virtual void PostCopy();
	virtual void PostPaste();

	//~ Begin UObject interface
	virtual void PostTransacted(const FTransactionObjectEvent& TransactionEvent) override;
	//~ End UObject interface

protected:
	virtual bool ShouldCreatePin(const UMovieGraphPin* InPin) const;
	void RebuildRuntimeEdgesFromPins();

	void CreatePins(const TArray<UMovieGraphPin*>& InInputPins, const TArray<UMovieGraphPin*>& InOutputPins);

	/** Recreate the pins on this node, discarding all existing pins. */
	void ReconstructPins();

	/** Update the position of the underlying runtime node to match the editor node. */
	void UpdatePosition() const;

	/**
	 * Update the comment bubble pin state on the underlying runtime node to match the editor node. Note that
	 * comment bubble visibility state and comment text are handled by UEdGraphNode overrides.
	 */
	void UpdateCommentBubblePinned() const;

protected:
	/** The runtime node that this editor node represents. */
	UPROPERTY()
	TObjectPtr<UMovieGraphNode> RuntimeNode;

	/** Should we early out in ReconstructNode(), skipping restoring the connections to other nodes. Set during Copy/Paste. */
	bool bDisableReconstructNode = false;
};

UCLASS()
class UMoviePipelineEdGraphNode : public UMoviePipelineEdGraphNodeBase
{
	GENERATED_BODY()

public:
	//~ Begin UEdGraphNode Interface
	virtual void AllocateDefaultPins() override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual void GetNodeContextMenuActions(UToolMenu* Menu, UGraphNodeContextMenuContext* Context) const override;
	//~ End UEdGraphNode Interface

protected:
	/** Toggle the promotion of the property with the given name to a pin. */
	void TogglePromotePropertyToPin(const FName PropertyName) const;

private:
	void GetPropertyPromotionContextMenuActions(UToolMenu* Menu, UGraphNodeContextMenuContext* Context) const;
};
