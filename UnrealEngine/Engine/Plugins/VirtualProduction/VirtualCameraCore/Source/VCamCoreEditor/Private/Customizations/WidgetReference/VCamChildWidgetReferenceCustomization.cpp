// Copyright Epic Games, Inc. All Rights Reserved.

#include "VCamChildWidgetReferenceCustomization.h"

#include "Algo/RemoveIf.h"
#include "Blueprint/UserWidget.h"
#include "UI/VCamWidget.h"

namespace UE::VCamCoreEditor::Private
{
	TSharedRef<IPropertyTypeCustomization> FVCamChildWidgetReferenceCustomization::MakeInstance()
	{
		return MakeShared<FVCamChildWidgetReferenceCustomization>();
	}

	TArray<UWidget*> FVCamChildWidgetReferenceCustomization::GetSelectableChildWidgets(TWeakObjectPtr<UUserWidget> Widget) const
	{
		TArray<UWidget*> Widgets = FChildWidgetReferenceCustomization::GetSelectableChildWidgets(Widget);
		Widgets.SetNum(Algo::RemoveIf(Widgets, [](UWidget* Widget){ return Cast<UVCamWidget>(Widget) == nullptr; }));
		return Widgets;
	}
}
