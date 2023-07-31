// Copyright Epic Games, Inc. All Rights Reserved.

#include "CommonHierarchicalScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "SCommonHierarchicalScrollBox.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CommonHierarchicalScrollBox)

/////////////////////////////////////////////////////
// UCommonHierarchicalScrollBox

UCommonHierarchicalScrollBox::UCommonHierarchicalScrollBox(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bAnimateWheelScrolling = true;
}

TSharedRef<SWidget> UCommonHierarchicalScrollBox::RebuildWidget()
{
	MyScrollBox = SNew(SCommonHierarchicalScrollBox)
		.Style(&WidgetStyle)
		.ScrollBarStyle(&WidgetBarStyle)
		.Orientation(Orientation)
		.ConsumeMouseWheel(ConsumeMouseWheel)
		.NavigationDestination(NavigationDestination)
		.NavigationScrollPadding(NavigationScrollPadding)
		.OnUserScrolled(BIND_UOBJECT_DELEGATE(FOnUserScrolled, SlateHandleUserScrolled))
		.AnimateWheelScrolling(bAnimateWheelScrolling);

	for ( UPanelSlot* PanelSlot : Slots )
	{
		if ( UScrollBoxSlot* TypedSlot = Cast<UScrollBoxSlot>(PanelSlot) )
		{
			TypedSlot->Parent = this;
			TypedSlot->BuildSlot(MyScrollBox.ToSharedRef());
		}
	}
	
	return MyScrollBox.ToSharedRef();
}
