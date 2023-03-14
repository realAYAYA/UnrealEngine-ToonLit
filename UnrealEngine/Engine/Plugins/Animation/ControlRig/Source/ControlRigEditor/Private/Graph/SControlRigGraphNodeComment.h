// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SGraphNodeComment.h"

class UEdGraphNode_Comment;

class SControlRigGraphNodeComment : public SGraphNodeComment
{
public:

	SControlRigGraphNodeComment();

	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void EndUserInteraction() const override;
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual void MoveTo(const FVector2D& NewPosition, FNodeSet& NodeFilter, bool bMarkDirty = true) override;

protected:

	virtual bool IsNodeUnderComment(UEdGraphNode_Comment* InCommentNode, const TSharedRef<SGraphNode> InNodeWidget) const override;
	bool IsNodeUnderComment(UEdGraphNode* InNode) const;
	bool IsNodeUnderComment(const FVector2D& InNodePosition) const;
	bool IsNodeUnderComment(const FVector2D& InCommentPosition, const FVector2D& InNodePosition) const;

private:

	FLinearColor CachedNodeCommentColor;

	int8 CachedColorBubble;
};
