// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "SNiagaraGraphNode.h"

class SNiagaraGraphParameterMapSetNode : public SNiagaraGraphNode
{
public:
	SLATE_BEGIN_ARGS(SNiagaraGraphParameterMapSetNode) {}
	SLATE_END_ARGS();

	void Construct(const FArguments& InArgs, UEdGraphNode* InGraphNode);
	virtual TSharedRef<SWidget> CreateNodeContentArea();

protected:
	FReply OnDroppedOnTarget(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent);
	bool OnAllowDrop(TSharedPtr<FDragDropOperation> DragDropOperation);

};