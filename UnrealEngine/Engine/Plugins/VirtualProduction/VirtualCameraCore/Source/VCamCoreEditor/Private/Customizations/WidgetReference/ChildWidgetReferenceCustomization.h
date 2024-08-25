// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IPropertyTypeCustomization.h"

class UUserWidget;
class UWidget;

namespace UE::VCamCoreEditor::Private
{
	class FChildWidgetReferenceCustomization : public IPropertyTypeCustomization
	{
	public:

		static TSharedRef<IPropertyTypeCustomization> MakeInstance();
		
		//~ Begin IDetailCustomization Interface
		virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
		virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override {}
		//~ End IDetailCustomization Interface

	protected:
		
		virtual TArray<UWidget*> GetSelectableChildWidgets(TWeakObjectPtr<UUserWidget> Widget) const;

	private:
		
		TArray<FString> GetPropertyItemList(TWeakObjectPtr<UUserWidget> Widget) const;
	};
}


