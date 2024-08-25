// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ChaosClothAsset/ImportedValueCustomization.h"

namespace UE::Chaos::ClothAsset
{
	/**
	 * Customization for all connectable property that could be imported
	 * Works like a FMathStructCustomization.
	 */
	class FConnectableValueCustomization : public FImportedValueCustomization
	{
	public:
		static TSharedRef<IPropertyTypeCustomization> MakeInstance();

		FConnectableValueCustomization();
		virtual ~FConnectableValueCustomization() override;

	protected:
		virtual void MakeHeaderRow(TSharedRef<class IPropertyHandle>& StructPropertyHandle, FDetailWidgetRow& Row) override;
		virtual TSharedRef<SWidget> MakeChildWidget(TSharedRef<IPropertyHandle>& StructurePropertyHandle, TSharedRef<IPropertyHandle>& PropertyHandle) override;
		
		static bool IsOverrideProperty(const TSharedPtr<IPropertyHandle>& Property);
		static bool IsOverridePropertyOf(const TSharedPtr<IPropertyHandle>& OverrideProperty, const TSharedPtr<IPropertyHandle>& Property);
		static bool BuildFabricMapsProperty(const TSharedPtr<IPropertyHandle>& Property);
		static bool CouldUseFabricsProperty(const TSharedPtr<IPropertyHandle>& Property);
		
	};
}
