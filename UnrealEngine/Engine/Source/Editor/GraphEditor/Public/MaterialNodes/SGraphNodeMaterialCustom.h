// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MaterialNodes/SGraphNodeMaterialBase.h"
#include "Text/HLSLSyntaxHighlighterMarshaller.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"

class GRAPHEDITOR_API SGraphNodeMaterialCustom : public SGraphNodeMaterialBase
{
public:
	SLATE_BEGIN_ARGS(SGraphNodeMaterialCustom){}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, class UMaterialGraphNode* InNode);

	UMaterialGraphNode* GetMaterialGraphNode() const {return MaterialNode;}

	FText GetHlslText() const;
	void OnCustomHlslTextCommited(const FText& InText, ETextCommit::Type InType);

protected:
	// SGraphNode interface
	virtual void CreateBelowPinControls(TSharedPtr<SVerticalBox> MainBox) override;
	virtual void CreateAdvancedViewArrow(TSharedPtr<SVerticalBox> MainBox) override;
	virtual EVisibility AdvancedViewArrowVisibility() const override;
	virtual void OnAdvancedViewChanged( const ECheckBoxState NewCheckedState ) override;
	virtual ECheckBoxState IsAdvancedViewChecked() const override;
	virtual const FSlateBrush* GetAdvancedViewArrow() const override;
	// End of SGraphNode interface

private:
	EVisibility CodeVisibility() const;

protected:
	/** Shader text box */
	TSharedPtr<FHLSLSyntaxHighlighterMarshaller> SyntaxHighlighter;
	TSharedPtr<SMultiLineEditableTextBox> ShaderTextBox;
};
