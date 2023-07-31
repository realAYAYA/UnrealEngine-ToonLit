// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Internationalization/Text.h"
#include "MaterialNodes/SGraphNodeMaterialBase.h"
#include "Math/Vector2D.h"
#include "SNodePanel.h"
#include "Templates/SharedPointer.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class SToolTip;
class SWidget;
class UEdGraph;
class UMaterialGraphNode_Composite;

//@TODO: This class is mostly C&P from UK2Node_Composite, consolidate composites to not be BP specific.
class GRAPHEDITOR_API SGraphNodeMaterialComposite : public SGraphNodeMaterialBase
{
public:
	SLATE_BEGIN_ARGS(SGraphNodeMaterialComposite){}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UMaterialGraphNode_Composite* InNode);

	// SGraphNode interface
	virtual void UpdateGraphNode() override;
	virtual TSharedPtr<SToolTip> GetComplexTooltip() override;
	// End of SGraphNode interface

	// SNodePanel::SNode interface
	virtual void MoveTo(const FVector2D& NewPosition, FNodeSet& NodeFilter, bool bMarkDirty = true) override;
	// End of SNodePanel::SNode interface
protected:
	virtual UEdGraph* GetInnerGraph() const;

private:
	FText GetPreviewCornerText() const;
	FText GetTooltipTextForNode() const;

	TSharedRef<SWidget> CreateNodeBody();

	/** Cached material graph node pointer to avoid casting */
	UMaterialGraphNode_Composite* CompositeNode;
};
