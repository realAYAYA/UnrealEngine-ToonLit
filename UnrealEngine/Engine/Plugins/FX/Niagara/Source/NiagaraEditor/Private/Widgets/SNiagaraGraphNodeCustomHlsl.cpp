// Copyright Epic Games, Inc. All Rights Reserved.

#include "SNiagaraGraphNodeCustomHlsl.h"
#include "NiagaraNodeCustomHlsl.h"
#include "NiagaraGraph.h"
#include "Widgets/SBoxPanel.h"
#include "GraphEditorSettings.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"

#define LOCTEXT_NAMESPACE "SNiagaraGraphNodeCustomHlsl"

SNiagaraGraphNodeCustomHlsl::SNiagaraGraphNodeCustomHlsl() : SNiagaraGraphNode()
{

}

void SNiagaraGraphNodeCustomHlsl::Construct(const FArguments& InArgs, UEdGraphNode* InGraphNode)
{
	GraphNode = InGraphNode;
	SyntaxHighlighter = FNiagaraHLSLSyntaxHighlighter::Create();
	RegisterNiagaraGraphNode(InGraphNode);
	UpdateGraphNode();
}

bool SNiagaraGraphNodeCustomHlsl::IsNameReadOnly() const
{
	return false;
}

void SNiagaraGraphNodeCustomHlsl::RequestRenameOnSpawn()
{
	// We only want to initiate the rename if this is a uniquely added node.
	UNiagaraNodeCustomHlsl* CustomNode = CastChecked<UNiagaraNodeCustomHlsl>(GraphNode);
	UNiagaraGraph* Graph = CastChecked<UNiagaraGraph>(CustomNode->GetGraph());
	TArray<UNiagaraNodeCustomHlsl*> CustomNodes;
	Graph->GetNodesOfClass<UNiagaraNodeCustomHlsl>(CustomNodes);

	int32 NumMatches = 0;
	for (UNiagaraNodeCustomHlsl* Node : CustomNodes)
	{
		if (Node == CustomNode)
		{
			continue;
		}

		bool bNeedsSync = Node->Signature.Name == CustomNode->Signature.Name;
		if (bNeedsSync)
		{
			NumMatches++;
		}
	}
	if (NumMatches == 0)
	{
		RequestRename();
	}
}

TSharedRef<SWidget> SNiagaraGraphNodeCustomHlsl::CreateTitleWidget(TSharedPtr<SNodeTitle> NodeTitle)
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SGraphNode::CreateTitleWidget(NodeTitle)
		];
}

void SNiagaraGraphNodeCustomHlsl::CreateBelowPinControls(TSharedPtr<SVerticalBox> MainBox)
{
	UNiagaraNodeCustomHlsl* CustomNode = CastChecked<UNiagaraNodeCustomHlsl>(GraphNode);

	TAttribute<FText> GetText;
	GetText.BindUObject(CustomNode, &UNiagaraNodeCustomHlsl::GetHlslText);
	
	FOnTextCommitted TextCommit;
	TextCommit.BindUObject(CustomNode, &UNiagaraNodeCustomHlsl::OnCustomHlslTextCommitted);

	SAssignNew(ShaderTextBox, SMultiLineEditableTextBox)
		.AutoWrapText(false)
		.Margin(FMargin(5, 5, 5, 5))
		.Text(GetText)
		.Marshaller(SyntaxHighlighter)
		.OnKeyCharHandler(this, &SNiagaraGraphNodeCustomHlsl::OnShaderTextKeyChar)
		.OnTextCommitted(TextCommit);
	MainBox->AddSlot()[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(FMargin(10.f, 5.f, 10.f, 10.f))
		[
			ShaderTextBox.ToSharedRef()
		]
	];
}

FReply SNiagaraGraphNodeCustomHlsl::OnShaderTextKeyChar(const FGeometry&, const FCharacterEvent& InCharacterEvent)
{
	const TCHAR Character = InCharacterEvent.GetCharacter();
	if (Character == TEXT('\t'))
	{
		// Convert tab to four spaces
		ShaderTextBox->InsertTextAtCursor(TEXT("    "));
		return FReply::Handled();
	}
	
	// Let SMultiLineEditableTextBox::OnKeyChar handle it.
	return FReply::Unhandled();
}

#undef LOCTEXT_NAMESPACE
