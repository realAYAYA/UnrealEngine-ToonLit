// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "IPropertyTypeCustomization.h"
#include "Internationalization/Text.h"
#include "Templates/SharedPointer.h"
#include "Types/SlateEnums.h"
#include "Widgets/Input/SComboBox.h"

class FDetailWidgetRow;
class IPropertyHandle;
class SWidget;

class FVCamInputProfileCustomization : public IPropertyTypeCustomization
{
public:
	FVCamInputProfileCustomization();

	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	/** IPropertyTypeCustomization instance */
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
	virtual void CustomizeChildren( TSharedRef<IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils ) override;

protected:
	TSharedPtr<IPropertyHandle> CachedStructPropertyHandle;
	TSharedPtr<IPropertyHandle> MappableKeyOverridesHandle;
	
	TSharedPtr<SComboBox<TSharedPtr<FString>>> ProfileComboBox;
	TArray<TSharedPtr<FString>> ProfileComboList;

	void BuildProfileComboList();

	bool IsProfileEnabled() const;
	void OnProfileChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);
	TSharedRef<SWidget> MakeProfileComboWidget(TSharedPtr<FString> InItem);
		
	FText GetProfileComboBoxContent() const;
	TSharedPtr<FString> GetProfileString(void* StructValueData) const;
};
