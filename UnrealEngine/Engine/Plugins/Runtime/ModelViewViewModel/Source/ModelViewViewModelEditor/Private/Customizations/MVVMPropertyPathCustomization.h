// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IPropertyTypeCustomization.h"
#include "MVVMBlueprintView.h"
#include "PropertyHandle.h"
#include "Types/MVVMFieldVariant.h"

namespace UE::MVVM
{

class SFieldSelector;
struct FBindingSource;

class FPropertyPathCustomization : public IPropertyTypeCustomization
{
public:
	FPropertyPathCustomization(UWidgetBlueprint* WidgetBlueprint);
	~FPropertyPathCustomization();

	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> InPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> InPropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override {}

	static TSharedRef<IPropertyTypeCustomization> MakeInstance(UWidgetBlueprint* InWidgetBlueprint)
	{
		check(InWidgetBlueprint != nullptr);

		return MakeShared<FPropertyPathCustomization>(InWidgetBlueprint);
	}

private:
	void OnFieldSelectionChanged(FMVVMBlueprintPropertyPath Selected);

	void OnOtherPropertyChanged();

	EMVVMBindingMode GetCurrentBindingMode() const;
	FBindingSource OnGetSelectedSource() const;

	FMVVMBlueprintPropertyPath OnGetSelectedField() const;

	void HandleBlueprintChanged(UBlueprint* Blueprint);

private:
	TSharedPtr<IPropertyHandle> PropertyHandle;
	UWidgetBlueprint* WidgetBlueprint = nullptr;

	TSharedPtr<SFieldSelector> FieldSelector;

	bool bIsWidget = false;
	bool bPropertySelectionChanging = false;

	FDelegateHandle OnBlueprintChangedHandle;
};

} // namespace UE::MVVM
