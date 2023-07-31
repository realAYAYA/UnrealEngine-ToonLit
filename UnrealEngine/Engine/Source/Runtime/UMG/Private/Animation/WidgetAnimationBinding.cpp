// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/WidgetAnimationBinding.h"
#include "UObject/Object.h"
#include "Components/Widget.h"
#include "Blueprint/WidgetTree.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(WidgetAnimationBinding)


/* FWidgetAnimationBinding interface
 *****************************************************************************/

UObject* FWidgetAnimationBinding::FindRuntimeObject(const UWidgetTree& WidgetTree, UUserWidget& UserWidget ) const
{	
	if (bIsRootWidget)
	{
		return &UserWidget;
	}

	UObject* FoundObject = WidgetTree.FindWidget(*WidgetName.ToString());

	if (FoundObject && (SlotWidgetName != NAME_None))
	{
		// if we were animating the slot, look up the slot that contains the widget 
		UWidget* WidgetObject = CastChecked<UWidget>(FoundObject);

		if (WidgetObject->Slot)
		{
			FoundObject = WidgetObject->Slot;
		}
	}

	return FoundObject;
}
