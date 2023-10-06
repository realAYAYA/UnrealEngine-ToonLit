// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMXControlConsoleEditorPortSelector.h"

#include "IO/DMXOutputPort.h"
#include "IO/DMXPortManager.h"

#include "SlateOptMacros.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"


#define LOCTEXT_NAMESPACE "SDMXControlConsoleEditorPortSelector"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SDMXControlConsoleEditorPortSelector::Construct(const FArguments& InArgs)
{
	OnPortsSelected = InArgs._OnPortsSelected;

	ChildSlot
	[
		SNew(SHorizontalBox)

		// Monitor All Ports Checkbox
		+ SHorizontalBox::Slot()
		.Padding(FMargin(8.f, 0.f, 0.f, 0.f))
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(STextBlock)
				.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				.Text(LOCTEXT("MonitorAllLabel", "Send Raw Data to All Ports"))
			]

			+ SHorizontalBox::Slot()
			.Padding(4.f, 0.f, 0.f, 0.f)
			.AutoWidth()
			[
				SAssignNew(SendToAllPortsCheckBox, SCheckBox)
				.OnCheckStateChanged(this, &SDMXControlConsoleEditorPortSelector::OnSendToAllPortsCheckBoxChanged)
				.IsChecked(InArgs._bSendToAllPorts)
			]
		]
		
		// Port Selector
		+ SHorizontalBox::Slot()
		.Padding(FMargin(20.f, 0.f, 0.f, 0.f))
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		[
			SAssignNew(PortSelectorWrapper, SHorizontalBox)

			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(SBox)
				.MinDesiredWidth(20.f)
				[
					SNew(STextBlock)
					.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
					.Text(LOCTEXT("PortLabel", "Port"))	
				]
			]

			+ SHorizontalBox::Slot()
			.Padding(4.f, 0.f, 0.f, 0.f)
			.AutoWidth()
			[
				SAssignNew(PortSelector, SDMXPortSelector)
				.Mode(EDMXPortSelectorMode::SelectFromAvailableOutputs)
				.InitialSelection(InArgs._InitiallySelectedGuid)
				.OnPortSelected(this, &SDMXControlConsoleEditorPortSelector::HandlePortSelected)
			]
		]
	];

	UpdateSelectedPorts();
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

bool SDMXControlConsoleEditorPortSelector::IsSendToAllPorts() const
{
	check(SendToAllPortsCheckBox.IsValid());

	return SendToAllPortsCheckBox->GetCheckedState() == ECheckBoxState::Checked;
}

FGuid SDMXControlConsoleEditorPortSelector::GetSelectedPortGuid() const
{
	check(PortSelector.IsValid());

	if (FDMXOutputPortSharedPtr SelectedOutputPort = PortSelector->GetSelectedOutputPort())
	{
		return SelectedOutputPort->GetPortGuid();
	}

	return FGuid();
}

void SDMXControlConsoleEditorPortSelector::UpdateSelectedPorts()
{
	check(SendToAllPortsCheckBox.IsValid());
	check(PortSelector.IsValid());
	check(PortSelectorWrapper.IsValid());

	SelectedOutputPorts.Reset();

	if (IsSendToAllPorts())
	{
		PortSelectorWrapper->SetVisibility(EVisibility::Collapsed);
		SelectedOutputPorts = FDMXPortManager::Get().GetOutputPorts();
	}
	else
	{
		PortSelectorWrapper->SetVisibility(EVisibility::Visible);

		if (FDMXOutputPortSharedPtr SharedOutputPort = PortSelector->GetSelectedOutputPort())
		{
			if (SharedOutputPort.IsValid())
			{
				SelectedOutputPorts.Add(SharedOutputPort.ToSharedRef());
			}
		}

		if (SelectedOutputPorts.Num() == 0 && FDMXPortManager::Get().GetOutputPorts().Num() > 0)
		{
			PortSelector->SelectPort(FDMXPortManager::Get().GetOutputPorts()[0]->GetPortGuid());
		}
	}
}

void SDMXControlConsoleEditorPortSelector::OnSendToAllPortsCheckBoxChanged(const ECheckBoxState NewCheckState)
{
	UpdateSelectedPorts();
	OnPortsSelected.ExecuteIfBound();
}

void SDMXControlConsoleEditorPortSelector::HandlePortSelected()
{
	UpdateSelectedPorts();
	OnPortsSelected.ExecuteIfBound();
}

#undef LOCTEXT_NAMESPACE
