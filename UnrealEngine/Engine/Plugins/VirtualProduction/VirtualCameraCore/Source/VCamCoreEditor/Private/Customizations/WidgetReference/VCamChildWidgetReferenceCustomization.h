// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ChildWidgetReferenceCustomization.h"

class UUserWidget;

namespace UE::VCamCoreEditor::Private
{
	class FVCamChildWidgetReferenceCustomization : public FChildWidgetReferenceCustomization
	{
	public:
		
		static TSharedRef<IPropertyTypeCustomization> MakeInstance();
		
	protected:
		
		//~ Begin FWidgetReferenceForBlueprintCustomization Interface
		virtual TArray<UWidget*> GetSelectableChildWidgets(TWeakObjectPtr<UUserWidget> Widget) const override;
		//~ End FWidgetReferenceForBlueprintCustomization Interface
	};
}


