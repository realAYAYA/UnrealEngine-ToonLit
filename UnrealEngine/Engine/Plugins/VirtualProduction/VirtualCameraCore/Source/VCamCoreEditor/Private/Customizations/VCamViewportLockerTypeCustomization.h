// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IPropertyTypeCustomization.h"

class UWidgetTree;
class UUserWidget;
class IDetailPropertyRow;

namespace UE::VCamCoreEditor::Private
{
	/** Just inlines the TMap (since the keys cannot be changed). */
	class FVCamViewportLockerTypeCustomization : public IPropertyTypeCustomization
	{
	public:

		static TSharedRef<IPropertyTypeCustomization> MakeInstance();

		//~ Begin IStructCustomization Interface
		virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
		virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
		//~ End IStructCustomization Interface
	};
}
