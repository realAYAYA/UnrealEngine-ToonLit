// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Math/Vector2D.h"
#include "SGraphNodeComment.h"
#include "SNodePanel.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class UMaterialGraphNode_Comment;

class SGraphNodeMaterialComment : public SGraphNodeComment
{
public:
	SLATE_BEGIN_ARGS(SGraphNodeMaterialComment){}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, class UMaterialGraphNode_Comment* InNode);

	// SNodePanel::SNode interface
	virtual void MoveTo(const FVector2D& NewPosition, FNodeSet& NodeFilter, bool bMarkDirty = true) override;
	// End of SNodePanel::SNode interface

private:
	/** Cached material graph node pointer to avoid casting */
	UMaterialGraphNode_Comment* CommentNode;
};
