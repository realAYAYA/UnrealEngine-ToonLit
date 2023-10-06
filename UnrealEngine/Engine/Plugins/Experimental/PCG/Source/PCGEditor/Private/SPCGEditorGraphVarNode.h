// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SPCGEditorGraphNode.h"
#include "Styling/SlateColor.h"
#include "Templates/SharedPointer.h"
#include "Types/SlateEnums.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class FText;
class SWidget;
class UK2Node;
struct FMargin;
struct FSlateBrush;

class SPCGEditorGraphVarNode : public SPCGEditorGraphNode
{
public:
	SLATE_BEGIN_ARGS(SPCGEditorGraphVarNode){}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UPCGEditorGraphNodeBase* InNode);

	virtual const FSlateBrush* GetShadowBrush(bool bSelected) const override;

	virtual void GetDiffHighlightBrushes(const FSlateBrush*& BackgroundOut, const FSlateBrush*& ForegroundOut) const override;

	// SGraphNode interface
	virtual void UpdateGraphNode() override;
	// End of SGraphNode interface

protected:
	FSlateColor GetVariableColor() const;
};
