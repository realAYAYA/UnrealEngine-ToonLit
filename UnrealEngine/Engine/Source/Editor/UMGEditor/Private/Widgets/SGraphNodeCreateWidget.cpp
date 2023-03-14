// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/SGraphNodeCreateWidget.h"

#include "Containers/Array.h"
#include "EdGraph/EdGraphPin.h"
#include "KismetPins/SGraphPinClass.h"
#include "Misc/AssertionMacros.h"
#include "Nodes/K2Node_CreateWidget.h"
#include "Templates/Casts.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class SGraphPin;

TSharedPtr<SGraphPin> SGraphNodeCreateWidget::CreatePinWidget(UEdGraphPin* Pin) const
{
	UK2Node_CreateWidget* CreateWidgetNode = CastChecked<UK2Node_CreateWidget>(GraphNode);
	UEdGraphPin* ClassPin = CreateWidgetNode->GetClassPin();
	if ((ClassPin == Pin) && (!ClassPin->bHidden || (ClassPin->LinkedTo.Num() > 0)))
	{
		TSharedPtr<SGraphPinClass> NewPin = SNew(SGraphPinClass, ClassPin);
		check(NewPin.IsValid());
		NewPin->SetAllowAbstractClasses(false);
		return NewPin;
	}
	return SGraphNodeK2Default::CreatePinWidget(Pin);
}
