// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaRundownRCPropertyItem.h"
#include "IPropertyRowGenerator.h"
#include "Rundown/DetailsView/RemoteControl/Properties/AvaRundownRCPropertyItem.h"
#include "SAvaRundownPageRemoteControlProps.h"
#include "Templates/SharedPointer.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SWidget.h"

class FText;

class SAvaRundownRCPropertyItemRow : public SMultiColumnTableRow<FAvaRundownRCPropertyItemPtr>
{
public:
	SLATE_BEGIN_ARGS(SAvaRundownRCPropertyItemRow) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedRef<SAvaRundownPageRemoteControlProps> InPropertyPanel,
		const TSharedRef<STableViewBase>& InOwnerTableView, const TSharedPtr<const FAvaRundownRCPropertyItem>& InRowItem);

	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& InColumnName) override;

	void UpdateValue();

protected:
	TWeakPtr<const FAvaRundownRCPropertyItem> ItemPtrWeak;
	TWeakPtr<SAvaRundownPageRemoteControlProps> PropertyPanelWeak;
	TSharedPtr<IPropertyRowGenerator> Generator;
	TSharedPtr<SBox> ValueContainer;
	TSharedPtr<SWidget> ValueWidget;

	/** Get this field's label. */
	FText GetFieldLabel() const;

	TSharedRef<SWidget> CreateValue();
};
