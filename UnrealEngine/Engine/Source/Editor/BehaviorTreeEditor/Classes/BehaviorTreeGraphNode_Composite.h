// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BehaviorTreeGraphNode.h"
#include "CoreMinimal.h"
#include "EdGraph/EdGraphNode.h"
#include "Internationalization/Text.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"

#include "BehaviorTreeGraphNode_Composite.generated.h"

class UObject;

UCLASS()
class BEHAVIORTREEEDITOR_API UBehaviorTreeGraphNode_Composite : public UBehaviorTreeGraphNode
{
	GENERATED_UCLASS_BODY()
	
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetDescription() const override;
	virtual FText GetTooltipText() const override;
	virtual bool RefreshNodeClass() override{ return false; }

	/** Gets a list of actions that can be done to this particular node */
	virtual void GetNodeContextMenuActions(class UToolMenu* Menu, class UGraphNodeContextMenuContext* Context) const override;

	/** check if node can accept breakpoints */
	virtual bool CanPlaceBreakpoints() const override { return true; }

	virtual FLinearColor GetBackgroundColor(bool bIsActiveForDebugger) const override;

protected:

	virtual void PostPasteNode() override;
};
