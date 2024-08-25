// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "IPropertyTypeCustomization.h"
#include "Input/Reply.h"
#include "Templates/SharedPointer.h"
#include "Styling/SlateTypes.h"

class FDetailWidgetRow;
class IDetailChildrenBuilder;
class IPropertyHandle;

class FRCAssetPathElementCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	//~ Begin IPropertyTypeCustomization
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> InPropertyHandle, FDetailWidgetRow& InHeaderRow, IPropertyTypeCustomizationUtils& InCustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> InPropertyHandle, IDetailChildrenBuilder& InChildBuilder, IPropertyTypeCustomizationUtils& InCustomizationUtils) override;
	//~ End IPropertyTypeCustomization

private:
	/** Returns if the CheckBox is checked or not */
	ECheckBoxState IsChecked() const;
	/** Callback called when you click on the CheckBox to enable/disable it */
	void OnCheckStateChanged(ECheckBoxState InNewState) const;
	/** Callback called when you click on the arrow button next to the entries to retrieve the path of the selected Asset */
	FReply OnGetAssetFromSelectionClicked() const;

private:
	TSharedPtr<SWidget> PathWidget;
	TSharedPtr<IPropertyHandle> ArrayEntryHandle;
	TSharedPtr<IPropertyHandle> IsInputHandle;
	TSharedPtr<IPropertyHandle> PathHandle;
};
