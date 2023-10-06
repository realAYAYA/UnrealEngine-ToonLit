// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "SGraphNodeKnot.h"

class RIGVMEDITOR_API SRigVMGraphNodeKnot : public SGraphNodeKnot
{
	SLATE_BEGIN_ARGS(SRigVMGraphNodeKnot) {} 
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphNode* InKnot);

	// SGraphNode interface
	virtual void EndUserInteraction() const override;
	virtual void MoveTo(const FVector2D& NewPosition, FNodeSet& NodeFilter, bool bMarkDirty = true) override;

	void HandleNodeBeginRemoval();
};