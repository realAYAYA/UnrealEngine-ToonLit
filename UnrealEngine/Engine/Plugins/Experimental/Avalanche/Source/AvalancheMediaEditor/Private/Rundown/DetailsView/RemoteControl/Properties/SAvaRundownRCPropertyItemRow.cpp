// Copyright Epic Games, Inc. All Rights Reserved.

#include "SAvaRundownRCPropertyItemRow.h"
#include "AvaRundownPageRemoteControlWidgetUtils.h"
#include "AvaRundownRCPropertyItem.h"
#include "IDetailTreeNode.h"
#include "Internationalization/Text.h"
#include "IPropertyRowGenerator.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "RemoteControlEntity.h"
#include "RemoteControlField.h"
#include "Rundown/DetailsView/RemoteControl/Properties/AvaRundownPageRemoteControlWidgetUtils.h"
#include "Widgets/Layout/SScissorRectBox.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SAvaRundownRCPropertyItemRow"

void SAvaRundownRCPropertyItemRow::Construct(const FArguments& InArgs, TSharedRef<SAvaRundownPageRemoteControlProps> InPropertyPanel,
	const TSharedRef<STableViewBase>& InOwnerTableView, const TSharedPtr<const FAvaRundownRCPropertyItem>& InRowItem)
{
	ItemPtrWeak = InRowItem;
	PropertyPanelWeak = InPropertyPanel;
	Generator = nullptr;
	ValueContainer = nullptr;
	ValueWidget = nullptr;

	SMultiColumnTableRow<FAvaRundownRCPropertyItemPtr>::Construct(FSuperRowType::FArguments(), InOwnerTableView);
}

TSharedRef<SWidget> SAvaRundownRCPropertyItemRow::GenerateWidgetForColumn(const FName& InColumnName)
{
	TSharedPtr<const FAvaRundownRCPropertyItem> ItemPtr = ItemPtrWeak.Pin();

	if (ItemPtr.IsValid())
	{
		if (InColumnName == SAvaRundownPageRemoteControlProps::PropertyColumnName)
		{
			return SNew(SScissorRectBox)
				[
					SNew(SBox)
					.VAlign(EVerticalAlignment::VAlign_Center)
					.Padding(3.f, 2.f, 3.f, 2.f)
					[
						SNew(STextBlock)
						.Text(GetFieldLabel())
					]
				];
		}
		else if (InColumnName == SAvaRundownPageRemoteControlProps::ValueColumnName)
		{
			return SAssignNew(ValueContainer, SBox)
				[
					CreateValue()
				];
		}
		else
		{
			TSharedPtr<SAvaRundownPageRemoteControlProps> PropertyPanel = PropertyPanelWeak.Pin();

			if (PropertyPanel.IsValid())
			{
				TSharedPtr<SWidget> Cell = nullptr;
				const TArray<FAvaRundownRCPropertyTableRowExtensionDelegate>& TableRowExtensionDelegates = PropertyPanel->GetTableRowExtensionDelegates(InColumnName);

				for (const FAvaRundownRCPropertyTableRowExtensionDelegate& TableRowExtensionDelegate : TableRowExtensionDelegates)
				{
					TableRowExtensionDelegate.ExecuteIfBound(PropertyPanel.ToSharedRef(), ItemPtr.ToSharedRef(), Cell);
				}

				if (Cell.IsValid())
				{
					return Cell.ToSharedRef();
				}
			}
		}
	}

	return SNullWidget::NullWidget;
}

void SAvaRundownRCPropertyItemRow::UpdateValue()
{
	if (ValueContainer.IsValid())
	{
		ValueContainer->SetContent(CreateValue());
	}
}

FText SAvaRundownRCPropertyItemRow::GetFieldLabel() const
{
	if (TSharedPtr<const FAvaRundownRCPropertyItem> ItemPtr = ItemPtrWeak.Pin())
	{
		if (TSharedPtr<FRemoteControlEntity> EntityPtr = ItemPtr->GetEntity())
		{
			TSharedPtr<FRemoteControlField> FieldPtr = StaticCastSharedPtr<FRemoteControlField>(EntityPtr);
			return FText::FromName(FieldPtr->FieldName);
		}
	}

	return FText::GetEmpty();
}

TSharedRef<SWidget> SAvaRundownRCPropertyItemRow::CreateValue()
{
	if (TSharedPtr<const FAvaRundownRCPropertyItem> ItemPtr = ItemPtrWeak.Pin())
	{
		if (TSharedPtr<FRemoteControlEntity> EntityPtr = ItemPtr->GetEntity())
		{
			TSharedPtr<FRemoteControlField> FieldPtr = StaticCastSharedPtr<FRemoteControlField>(EntityPtr);

			// For the moment, just use the first object.
			TArray<UObject*> Objects = FieldPtr->GetBoundObjects();

			if ((FieldPtr->FieldType == EExposedFieldType::Property) && (Objects.Num() > 0))
			{
				FPropertyRowGeneratorArgs Args;
				Generator = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor").CreatePropertyRowGenerator(Args);
				Generator->SetObjects({Objects[0]});

				if (TSharedPtr<IDetailTreeNode> Node = FAvaRundownPageRemoteControlWidgetUtils::FindNode(Generator->GetRootTreeNodes(), FieldPtr->FieldPathInfo.ToPathPropertyString(), FAvaRundownPageRemoteControlWidgetUtils::EFindNodeMethod::Path))
				{
					const FNodeWidgets NodeWidgets = Node->CreateNodeWidgets();
					ValueWidget = NodeWidgets.WholeRowWidget.IsValid() ? NodeWidgets.WholeRowWidget : NodeWidgets.ValueWidget;

					if (ItemPtr->IsEntityControlled())
					{
						ValueWidget = SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.VAlign(EVerticalAlignment::VAlign_Center)
							[
								ValueWidget.ToSharedRef()
							]
							+ SHorizontalBox::Slot()
							.VAlign(EVerticalAlignment::VAlign_Center)
							.Padding(3.f, 0.f, 0.f, 0.f)
							[
								SNew(STextBlock)
								.Text(LOCTEXT("Controlled", "(Controlled)"))
							];

						ValueWidget->SetEnabled(false);
					}

					return ValueWidget.ToSharedRef();
				}
			}
		}
	}

	return SNullWidget::NullWidget;
}

#undef LOCTEXT_NAMESPACE
