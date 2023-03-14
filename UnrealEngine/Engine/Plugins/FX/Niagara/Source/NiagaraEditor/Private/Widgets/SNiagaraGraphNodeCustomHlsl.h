// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/NiagaraHLSLSyntaxHighlighter.h"
#include "SNiagaraGraphNode.h"

class SMultiLineEditableTextBox;

/** A graph node widget representing a niagara input node. */
class SNiagaraGraphNodeCustomHlsl : public SNiagaraGraphNode
{
public:
	SLATE_BEGIN_ARGS(SNiagaraGraphNodeCustomHlsl) {}
	SLATE_END_ARGS();

	SNiagaraGraphNodeCustomHlsl();

	void Construct(const FArguments& InArgs, UEdGraphNode* InGraphNode);

	//~ SGraphNode api
	virtual TSharedRef<SWidget> CreateTitleWidget(TSharedPtr<SNodeTitle> NodeTitle) override;
	virtual bool IsNameReadOnly() const override;
	virtual void RequestRenameOnSpawn() override;
	virtual bool ShouldAllowCulling() const override { return false; }

	virtual void CreateBelowPinControls(TSharedPtr<SVerticalBox> MainBox);
protected:

	TSharedPtr<FNiagaraHLSLSyntaxHighlighter> SyntaxHighlighter;
	TSharedPtr<SMultiLineEditableTextBox> ShaderTextBox;

private:
	FReply OnShaderTextKeyChar(const FGeometry& MyGeometry, const FCharacterEvent& InCharacterEvent);
};
