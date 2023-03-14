// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SGraphNode.h"
#include "NiagaraNode.h"

/** A graph node widget representing a niagara node. */
class SNiagaraGraphNode : public SGraphNode
{
public:
	SLATE_BEGIN_ARGS(SNiagaraGraphNode) {}
	SLATE_END_ARGS();

	SNiagaraGraphNode();
	virtual ~SNiagaraGraphNode();

	void Construct(const FArguments& InArgs, UEdGraphNode* InGraphNode);

	//~ SGraphNode api
	virtual void UpdateGraphNode() override;
	virtual void CreateInputSideAddButton(TSharedPtr<SVerticalBox> InputBox) override;
	virtual void CreateOutputSideAddButton(TSharedPtr<SVerticalBox> OutputBox) override;
	virtual void UpdateErrorInfo() override;

protected:
	void RegisterNiagaraGraphNode(UEdGraphNode* InNode);
	void HandleNiagaraNodeChanged(UNiagaraNode* InNode);
	TWeakObjectPtr<UNiagaraNode> NiagaraNode;
	FGuid LastSyncedNodeChangeId;
};