// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DMXProtocolCommon.h"
#include "Widgets/SDMXPortSelector.h"

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Input/SComboBox.h"

enum class EDMXPortSelectorMode : uint8;
class SDMXPortSelector;

class SCheckBox;
class SHorizontalBox;
class STextBlock;


/** Widget to select Ports to monitor */
class SDMXOutputConsolePortSelector
	: public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDMXOutputConsolePortSelector)
		: _bSendToAllPorts(true)
		, _InitiallySelectedGuid(FGuid())
	{}

		SLATE_ARGUMENT(bool, bSendToAllPorts)

		SLATE_ARGUMENT(FGuid, InitiallySelectedGuid)

		SLATE_EVENT(FSimpleDelegate, OnPortsSelected)

	SLATE_END_ARGS()

	/** Constructs the widget */
	void Construct(const FArguments& InArgs);

	/** Returns true if the Sent To All Ports CheckBox is checked */
	bool IsSendToAllPorts() const;

	/** Returns the Guid of the selected port */
	FGuid GetSelectedPortGuid() const;

	FORCEINLINE const TArray<FDMXOutputPortSharedRef>& GetSelectedOutputPorts() const { return SelectedOutputPorts; }

private:
	/** Updates the selected ports */
	void UpdateSelectedPorts();

	/** Called when the Monitor All Ports Check Box state changed */
	void OnSendToAllPortsCheckBoxChanged(const ECheckBoxState NewCheckState);

	/** Called when a port was selected in the Port Selector */
	void HandlePortSelected();

	/** The output ports that are currently being monitored */
	TArray<FDMXOutputPortSharedRef> SelectedOutputPorts;

	/** CheckBox to toggle if all ports should be monitored */
	TSharedPtr<SCheckBox> SendToAllPortsCheckBox;

	/** Horizontal box that contains the Port Selector and its Label */
	TSharedPtr<SHorizontalBox> PortSelectorWrapper;

	/** Selector for the port to monitor */
	TSharedPtr<SDMXPortSelector> PortSelector;

	FSimpleDelegate OnPortsSelected;
};
