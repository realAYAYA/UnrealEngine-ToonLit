// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMXControlConsoleDataDetails.h"

#include "DMXControlConsoleData.h"
#include "DMXControlConsoleEditorManager.h"
#include "Widgets/SDMXControlConsoleEditorPortSelector.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IPropertyUtilities.h"
#include "PropertyHandle.h"


#define LOCTEXT_NAMESPACE "DMXControlConsoleDataDetails"

void FDMXControlConsoleDataDetails::CustomizeDetails(IDetailLayoutBuilder& InDetailLayout)
{
	PropertyUtilities = InDetailLayout.GetPropertyUtilities();

	IDetailCategoryBuilder& ControlConsoleCategory = InDetailLayout.EditCategory("DMX Control Console", FText::GetEmpty());
	const TSharedPtr<IPropertyHandle> FaderGroupRowsHandle = InDetailLayout.GetProperty(UDMXControlConsoleData::GetFaderGroupRowsPropertyName());
	InDetailLayout.HideProperty(FaderGroupRowsHandle);
	
	const TSharedPtr<IPropertyHandle> DMXLibraryHandle = InDetailLayout.GetProperty(UDMXControlConsoleData::GetDMXLibraryPropertyName());
	InDetailLayout.HideProperty(DMXLibraryHandle);
	ControlConsoleCategory.AddProperty(DMXLibraryHandle);
	DMXLibraryHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FDMXControlConsoleDataDetails::ForceRefresh));
	DMXLibraryHandle->SetOnChildPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FDMXControlConsoleDataDetails::ForceRefresh));

	GeneratePortSelectorRow(InDetailLayout);
}

void FDMXControlConsoleDataDetails::GeneratePortSelectorRow(IDetailLayoutBuilder& InDetailLayout)
{
	IDetailCategoryBuilder& ControlConsoleCategory = InDetailLayout.EditCategory("DMX Control Console", FText::GetEmpty());

	ControlConsoleCategory.AddCustomRow(FText::GetEmpty())
		.WholeRowContent()
		[
			SAssignNew(PortSelector, SDMXControlConsoleEditorPortSelector)
			.OnPortsSelected(this, &FDMXControlConsoleDataDetails::OnSelectedPortsChanged)
		];

	OnSelectedPortsChanged();
}

void FDMXControlConsoleDataDetails::ForceRefresh() const
{
	if (!PropertyUtilities.IsValid())
	{
		return;
	}
	
	PropertyUtilities->ForceRefresh();
}

void FDMXControlConsoleDataDetails::OnSelectedPortsChanged()
{
	UDMXControlConsoleData* ConsoleData = FDMXControlConsoleEditorManager::Get().GetEditorConsoleData();
	if (!ConsoleData)
	{
		return;
	}

	if (!PortSelector.IsValid())
	{
		return;
	}

	const TArray<FDMXOutputPortSharedRef> SelectedOutputPorts = PortSelector->GetSelectedOutputPorts();
	ConsoleData->UpdateOutputPorts(SelectedOutputPorts);
}

#undef LOCTEXT_NAMESPACE
