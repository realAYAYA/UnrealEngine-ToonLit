// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Math/Vector2D.h"
#include "SGraphNode.h"
#include "SNodePanel.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class FGraphNodeMetaData;
class UMaterialGraphNode_Root;

class GRAPHEDITOR_API SGraphNodeMaterialResult : public SGraphNode
{
public:
	SLATE_BEGIN_ARGS(SGraphNodeMaterialResult){}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, class UMaterialGraphNode_Root* InNode);

	// SGraphNode interface
	virtual void CreatePinWidgets() override;
	virtual void PopulateMetaTag(FGraphNodeMetaData* TagMeta) const override;
	// End of SGraphNode interface

	// SNodePanel::SNode interface
	virtual void MoveTo(const FVector2D& NewPosition, FNodeSet& NodeFilter, bool bMarkDirty = true) override;
	// End of SNodePanel::SNode interface

private:
	UMaterialGraphNode_Root* RootNode;
};
