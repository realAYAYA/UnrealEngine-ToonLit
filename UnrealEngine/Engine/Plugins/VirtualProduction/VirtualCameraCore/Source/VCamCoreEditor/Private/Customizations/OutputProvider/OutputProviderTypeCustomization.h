// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IPropertyTypeCustomization.h"

class IDetailGroup;
class IPropertyUtilities;
class UVCamWidget;
class UVCamOutputProviderBase;

namespace UE::VCamCoreEditor::Private
{
	/** Needed to customize display UPROPERTY(Instanced) instances. Causes FOutputProviderLayoutCustomization to be used. */
	class FOutputProviderTypeCustomization
		: public IPropertyTypeCustomization
	{
	public:

		static TSharedRef<IPropertyTypeCustomization> MakeInstance();

		//~ Begin IDetailCustomization Interface
		virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
		virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
		//~ End IDetailCustomization Interface
	};
}