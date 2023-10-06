// Copyright Epic Games, Inc. All Rights Reserved.

#include "Types/UIFSlotBase.h"
#include "UIFWidget.h"
//#include "UIFManagerSubsystem.h"


#include UE_INLINE_GENERATED_CPP_BY_NAME(UIFSlotBase)


/**
 *
 */
void FUIFrameworkSlotBase::AuthoritySetWidget(UUIFrameworkWidget* InWidget)
{
	Widget = InWidget;
	WidgetId = Widget ? Widget->GetWidgetId() : FUIFrameworkWidgetId();
}
