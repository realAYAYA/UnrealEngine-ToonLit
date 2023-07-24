// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "IPropertyTypeCustomization.h"
#include "PropertyHandle.h"
#include "DetailWidgetRow.h"


namespace UE::MLDeformer
{
	/**
	 * The ML Deformer morph model's quality level property detail customization.
	 * @see FMLDeformerMorphModelQualityLevel
	 */
	class MLDEFORMERFRAMEWORKEDITOR_API FMLDeformerMorphModelQualityLevelCustomization
		: public IPropertyTypeCustomization
	{
	public:
		static TSharedRef<IPropertyTypeCustomization> MakeInstance();

		// IPropertyTypeCustomization overrides.
		virtual void CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
		virtual void CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override {}
		// ~END IPropertyTypeCustomization overrides.
	};
}	// namespace UE::MLDeformer
