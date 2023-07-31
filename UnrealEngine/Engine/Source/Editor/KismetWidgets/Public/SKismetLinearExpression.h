// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Set.h"
#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Templates/SharedPointer.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SWidget.h"

class SWidget;
class UEdGraphNode;
class UEdGraphPin;

//////////////////////////////////////////////////////////////////////////
// SKismetLinearExpression

class KISMETWIDGETS_API SKismetLinearExpression : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SKismetLinearExpression )
		: _IsEditable(true)
		{}

		SLATE_ATTRIBUTE( bool, IsEditable )
	SLATE_END_ARGS()
public:
	void Construct(const FArguments& InArgs, UEdGraphPin* InitialInputPin);

	void SetExpressionRoot(UEdGraphPin* InputPin);
protected:
	TSharedRef<SWidget> MakeNodeWidget(const UEdGraphNode* Node, const UEdGraphPin* FromPin);
	TSharedRef<SWidget> MakePinWidget(const UEdGraphPin* Pin);

protected:
	TAttribute<bool> IsEditable;
	TSet<const UEdGraphNode*> VisitedNodes;
};
