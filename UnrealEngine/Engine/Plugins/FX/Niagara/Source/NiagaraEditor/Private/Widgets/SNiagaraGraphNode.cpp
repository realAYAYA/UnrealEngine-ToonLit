// Copyright Epic Games, Inc. All Rights Reserved.

#include "SNiagaraGraphNode.h"
#include "GraphEditorSettings.h"
#include "Rendering/DrawElements.h"
#include "SGraphPin.h"
#include "NiagaraEditorSettings.h"

#define LOCTEXT_NAMESPACE "SNiagaraGraphNode"

SNiagaraGraphNode::SNiagaraGraphNode() : SGraphNode()
{
	NiagaraNode = nullptr;
}

SNiagaraGraphNode::~SNiagaraGraphNode()
{
	if (NiagaraNode.IsValid())
	{
		NiagaraNode->OnVisualsChanged().RemoveAll(this);
	}
}

void SNiagaraGraphNode::Construct(const FArguments& InArgs, UEdGraphNode* InGraphNode)
{
	GraphNode = InGraphNode;
	RegisterNiagaraGraphNode(InGraphNode);
	UpdateGraphNode();
	
}


void SNiagaraGraphNode::HandleNiagaraNodeChanged(UNiagaraNode* InNode)
{
	check(InNode == NiagaraNode);
	UpdateGraphNode();
}

void SNiagaraGraphNode::RegisterNiagaraGraphNode(UEdGraphNode* InNode)
{
	NiagaraNode = Cast<UNiagaraNode>(InNode);
	NiagaraNode->OnVisualsChanged().AddSP(this, &SNiagaraGraphNode::HandleNiagaraNodeChanged);
}

void SNiagaraGraphNode::UpdateGraphNode()
{
	check(NiagaraNode.IsValid());
	SGraphNode::UpdateGraphNode();
	LastSyncedNodeChangeId = NiagaraNode->GetChangeId();
}

void SNiagaraGraphNode::CreateInputSideAddButton(TSharedPtr<SVerticalBox> InputBox)
{
	NiagaraNode->AddWidgetsToInputBox(InputBox);
}

void SNiagaraGraphNode::CreateOutputSideAddButton(TSharedPtr<SVerticalBox> OutputBox)
{
	NiagaraNode->AddWidgetsToOutputBox(OutputBox);
}

void SNiagaraGraphNode::UpdateErrorInfo()
{
	const UNiagaraEditorSettings* NiagaraEditorSettings = GetDefault<UNiagaraEditorSettings>();
	if (GraphNode != nullptr && GraphNode->IsA<UNiagaraNode>() && NiagaraEditorSettings->IsAllowedClass(GraphNode->GetClass()) == false)
	{
		ErrorMsg = FString(TEXT("UNSUPPORTED!"));
		ErrorColor = FAppStyle::GetColor("ErrorReporting.BackgroundColor");
	}
	else
	{
		SGraphNode::UpdateErrorInfo();
	}
}

#undef LOCTEXT_NAMESPACE