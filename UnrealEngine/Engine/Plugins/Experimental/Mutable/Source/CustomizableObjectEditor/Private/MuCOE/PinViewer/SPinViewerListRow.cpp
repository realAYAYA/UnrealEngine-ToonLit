// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/PinViewer/SPinViewerListRow.h"

#include "Containers/Map.h"
#include "Delegates/Delegate.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Text.h"
#include "Math/Color.h"
#include "Misc/Attribute.h"
#include "MuCOE/EdGraphSchema_CustomizableObject.h"
#include "MuCOE/Nodes/CustomizableObjectNode.h"
#include "MuCOE/PinViewer/SPinViewer.h"
#include "ScopedTransaction.h"
#include "SlotBase.h"
#include "Styling/SlateColor.h"
#include "Types/SlateEnums.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"

class STableViewBase;
class SWidget;

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


void SPinViewerListRow::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTableView)
{
	PinViewer = InArgs._PinViewer;
	PinReference = InArgs._PinReference;

	PinId = PinReference.Get()->PinId;
	
	SMutableExpandableTableRow<TSharedPtr<FEdGraphPinReference>>::Construct(FSuperRowType::FArguments(), OwnerTableView);
}


TSharedRef<SWidget> SPinViewerListRow::GenerateWidgetForColumn(const FName& ColumnName)
{
	const UEdGraphPin* Pin = PinReference.Get();
	
	if (ColumnName == SPinViewer::COLUMN_NAME)
	{
		const TSharedRef<STextBlock> TextBlock = SNew(STextBlock)
			.Text(SPinViewer::GetPinName(*Pin));
		
		if (Pin->bOrphanedPin)
		{
			TextBlock->SetColorAndOpacity(FLinearColor::Red);
		}
		
		return SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(4).VAlign(VAlign_Center)
			[
				TextBlock
			];
	}
	else if (ColumnName == SPinViewer::COLUMN_TYPE)
	{
		return SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(4).VAlign(VAlign_Center).HAlign(HAlign_Center)
			[
				SNew(STextBlock).Text(UEdGraphSchema_CustomizableObject::GetPinCategoryName(Pin->PinType.PinCategory))
			];
	}
	else if (ColumnName == SPinViewer::COLUMN_VISIBILITY)
	{
		return SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(4).VAlign(VAlign_Center).HAlign(HAlign_Center)
			[
				SNew(SCheckBox)
				.OnCheckStateChanged(this, &SPinViewerListRow::OnPinVisibilityCheckStateChanged)
				.IsChecked(this, &SPinViewerListRow::IsVisibilityChecked)
				.IsEnabled(PinViewer->Node->CanPinBeHidden(*Pin))
			];
	}

	return SNullWidget::NullWidget;
}


TSharedPtr<SWidget> SPinViewerListRow::GenerateAdditionalWidgetForRow()
{
	return PinViewer->Node->CustomizePinDetails(*PinReference.Get());
}


EVisibility SPinViewerListRow::GetAdditionalWidgetDefaultVisibility() const
{
	if (const EVisibility* Result = PinViewer->AdditionalWidgetVisibility.Find(PinId))
	{
		return *Result;
	}
	else
	{
		return EVisibility::Collapsed;
	}
}


void SPinViewerListRow::SetAdditionalWidgetVisibility(const EVisibility InVisibility)
{
	SMutableExpandableTableRow<TSharedPtr<FEdGraphPinReference, ESPMode::ThreadSafe>>::SetAdditionalWidgetVisibility(InVisibility);

	PinViewer->AdditionalWidgetVisibility.Add(PinId, InVisibility);
}


void SPinViewerListRow::OnPinVisibilityCheckStateChanged(ECheckBoxState NewRadioState)
{
	FScopedTransaction Transaction(LOCTEXT("ChangedPinVisiblilityTransaction", "Changed Pin Visiblility"));
	PinViewer->Node->Modify();
	PinViewer->Node->SetPinHidden(*PinReference.Get(), NewRadioState == ECheckBoxState::Unchecked);
}


ECheckBoxState SPinViewerListRow::IsVisibilityChecked() const
{
	return PinReference.Get()->bHidden ? ECheckBoxState::Unchecked : ECheckBoxState::Checked;
}


#undef LOCTEXT_NAMESPACE
