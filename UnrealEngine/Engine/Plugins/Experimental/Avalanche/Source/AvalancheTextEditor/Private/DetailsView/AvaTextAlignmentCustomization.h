// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IPropertyTypeCustomization.h"
#include "Templates/SharedPointer.h"

enum class EText3DHorizontalTextAlignment : uint8;
enum class EText3DVerticalTextAlignment : uint8;

class FAvaTextAlignmentCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	// ~ Begin IPropertyTypeCustomization interface
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle,
         class FDetailWidgetRow& HeaderRow,
         IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
	
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle,
		class IDetailChildrenBuilder& StructBuilder,
		IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
	// ~ End IPropertyTypeCustomization interface

protected:
	TSharedPtr<IPropertyHandle> AlignmentPropertyHandle;

	void OnHorizontalAlignmentChanged(EText3DHorizontalTextAlignment InHorizontalAlignment) const;
	void OnVerticalAlignmentChanged(EText3DVerticalTextAlignment InVerticalAlignment) const;
};
