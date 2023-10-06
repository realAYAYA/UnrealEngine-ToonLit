// Copyright Epic Games, Inc. All Rights Reserved.

#include "Blueprint/WidgetChild.h"

#include "CoreMinimal.h"
#include "Blueprint/WidgetTree.h"


FWidgetChild::FWidgetChild()
{}

FWidgetChild::FWidgetChild(const class UUserWidget* Outer, FName InWidgetName)
	:WidgetName(InWidgetName)
{
	if (!WidgetName.IsNone() && Outer && Outer->WidgetTree)
	{
		WidgetPtr = Outer->WidgetTree->FindWidget(WidgetName);
	}
}

UWidget* FWidgetChild::Resolve(const UWidgetTree* WidgetTree)
{
	if (!WidgetName.IsNone() && WidgetTree)
	{
		WidgetPtr = WidgetTree->FindWidget(WidgetName);		
	}
	else
	{
		WidgetPtr = nullptr;
	}
	
	return WidgetPtr.Get();
}
