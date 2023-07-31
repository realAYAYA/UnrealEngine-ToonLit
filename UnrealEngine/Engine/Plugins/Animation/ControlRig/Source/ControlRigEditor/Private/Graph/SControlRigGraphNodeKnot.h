// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "SGraphNodeKnot.h"

class SControlRigGraphNodeKnot : public SGraphNodeKnot
{
	SLATE_BEGIN_ARGS(SControlRigGraphNodeKnot) {} 
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphNode* InKnot);

	// SGraphNode interface
	virtual void EndUserInteraction() const override;
	virtual void MoveTo(const FVector2D& NewPosition, FNodeSet& NodeFilter, bool bMarkDirty = true) override;
};