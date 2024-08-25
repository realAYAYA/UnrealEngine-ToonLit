// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IPropertyTypeCustomization.h"

class UWidgetTree;
class UUserWidget;
class IDetailPropertyRow;

namespace UE::VCamCoreEditor::Private
{
	/** Makes the "Virtual Camera" category appear first on the actor properties. */
	class FWidgetConnectionConfigTypeCustomization : public IPropertyTypeCustomization
	{
	public:

		static TSharedRef<IPropertyTypeCustomization> MakeInstance();

		//~ Begin IStructCustomization Interface
		virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
		virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
		//~ End IStructCustomization Interface

	private:

		void CustomizeConnectionTargetsReferenceProperty(TSharedRef<IPropertyHandle> StructPropertyHandle, TSharedRef<IPropertyHandle> ConnectionTargetsPropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) const;
		TAttribute<TArray<FName>> CreateGetConnectionsFromChildWidgetAttribute(TSharedRef<IPropertyHandle> StructPropertyHandle, IPropertyTypeCustomizationUtils& CustomizationUtils) const;
	};
}

