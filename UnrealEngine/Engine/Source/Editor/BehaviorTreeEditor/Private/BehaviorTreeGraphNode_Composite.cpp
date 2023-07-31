// Copyright Epic Games, Inc. All Rights Reserved.

#include "BehaviorTreeGraphNode_Composite.h"

#include "BehaviorTree/BTCompositeNode.h"
#include "BehaviorTree/BTNode.h"
#include "HAL/PlatformMath.h"
#include "Internationalization/Internationalization.h"
#include "Templates/Casts.h"

class UToolMenu;

UBehaviorTreeGraphNode_Composite::UBehaviorTreeGraphNode_Composite(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FText UBehaviorTreeGraphNode_Composite::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	const UBTNode* MyNode = Cast<UBTNode>(NodeInstance);
	if (MyNode != NULL)
	{
		return FText::FromString(MyNode->GetNodeName());
	}
	return Super::GetNodeTitle(TitleType);
}

FText UBehaviorTreeGraphNode_Composite::GetDescription() const
{
	const UBTCompositeNode* CompositeNode = Cast<UBTCompositeNode>(NodeInstance);
	if (CompositeNode && CompositeNode->IsApplyingDecoratorScope())
	{
		return FText::Format(FText::FromString(TEXT("{0}\n{1}")),
			Super::GetDescription(),
			NSLOCTEXT("BehaviorTreeEditor", "CompositeNodeScopeDesc", "Local scope for observers"));
	}

	return Super::GetDescription();
}

FText UBehaviorTreeGraphNode_Composite::GetTooltipText() const
{
	const UBTCompositeNode* CompositeNode = Cast<UBTCompositeNode>(NodeInstance);
	if (CompositeNode && CompositeNode->IsApplyingDecoratorScope())
	{
		return FText::Format(FText::FromString(TEXT("{0}\n\n{1}")), 
			Super::GetDescription(), 
			NSLOCTEXT("BehaviorTreeEditor", "CompositeNodeScopeTooltip", "This node is a local scope for decorators.\nAll observing decorators (Lower Priority or Both) will be removed when execution flow leaves this branch."));
	}

	return Super::GetTooltipText();
}

void UBehaviorTreeGraphNode_Composite::GetNodeContextMenuActions(UToolMenu* Menu, UGraphNodeContextMenuContext* Context) const
{
	AddContextMenuActionsDecorators(Menu, "BehaviorTreeGraphNode", Context);
	AddContextMenuActionsServices(Menu, "BehaviorTreeGraphNode", Context);
}
