// Copyright Epic Games, Inc. All Rights Reserved.

#include "CustomDetailsViewCustomItem.h"
#include "Internationalization/Text.h"
#include "CustomDetailsViewItemBase.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "CustomDetailsViewCustomItem"

FCustomDetailsViewCustomItem::FCustomDetailsViewCustomItem(const TSharedRef<SCustomDetailsView>& InCustomDetailsView
	, const TSharedPtr<ICustomDetailsViewItem>& InParentItem, FName InItemName, const FText& InLabel, const FText& InToolTip)
	: FCustomDetailsViewItemBase(InCustomDetailsView, InParentItem)
	, ItemName(InItemName)
	, Label(InLabel)
	, ToolTip(InToolTip)
{
	InitWidget();
}

void FCustomDetailsViewCustomItem::InitWidget()
{
	CreateNameWidget();
	SetValueWidget(SNullWidget::NullWidget);
}

void FCustomDetailsViewCustomItem::CreateNameWidget()
{
	DetailWidgetRow.NameContent()
		[
			SNew(STextBlock)
				.Text(Label)
				.ToolTipText(ToolTip)
				.Font(FAppStyle::GetFontStyle("PropertyWindow.NormalFont"))
				.ShadowOffset(FVector2D(1.0f, 1.0f))
		];
}

void FCustomDetailsViewCustomItem::RefreshItemId()
{
	ItemId = FCustomDetailsViewItemId::MakePropertyId(ItemName);
}

void FCustomDetailsViewCustomItem::SetLabel(const FText& InLabel)
{
	Label = InLabel;
	CreateNameWidget();
}

void FCustomDetailsViewCustomItem::SetToolTip(const FText& InToolTip)
{
	ToolTip = InToolTip;
	CreateNameWidget();
}

void FCustomDetailsViewCustomItem::SetValueWidget(const TSharedRef<SWidget>& InValueWidget)
{
	DetailWidgetRow.ValueContent()
		[
			InValueWidget
		];
}

void FCustomDetailsViewCustomItem::SetExtensionWidget(const TSharedRef<SWidget>& InExpansionWidget)
{
	DetailWidgetRow.ExtensionContent()
		[
			InExpansionWidget
		];
}

TSharedRef<ICustomDetailsViewItem> FCustomDetailsViewCustomItem::AsItem()
{
	return SharedThis(this);
}

#undef LOCTEXT_NAMESPACE
