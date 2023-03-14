// Copyright Epic Games, Inc. All Rights Reserved.

#include "SWebAPISchemaOperationRequestRow.h"

#include "WebAPIEditorStyle.h"
#include "Details/ViewModels/WebAPIEnumViewModel.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Widgets/Input/SHyperlink.h"
#include "Widgets/Layout/SBox.h"

void SWebAPISchemaOperationRequestRow::Construct(const FArguments& InArgs, const TSharedRef<FWebAPIOperationRequestViewModel>& InViewModel, const TSharedRef<STableViewBase>& InOwnerTableView)
{
	SWebAPISchemaTreeTableRow::Construct(
		SWebAPISchemaTreeTableRow::FArguments()
		.Content()
		[
			SNew(SBorder)
			.ToolTipText(InViewModel->GetTooltip())
			.BorderBackgroundColor(FLinearColor(0,0,0,0))
			.Padding(4)
			[
				SNew(SBox)
				[
					SNew(SHorizontalBox)
								
					+ SHorizontalBox::Slot()
					.Padding(0)
					.FillWidth(1.0f)
					[
						SNew(SBorder)
						.BorderBackgroundColor(FLinearColor(0,0,0,0))
						[
							SNew(SBox)
							[
								SNew(SHorizontalBox)
								
								+ SHorizontalBox::Slot()
								.Padding(9, 0, 0, 1)
								.VAlign(VAlign_Center)
								[
									SNew(STextBlock)
									.Text(InViewModel->GetLabel())
									.Font(FAppStyle::Get().GetFontStyle("NormalFontBold"))
								]
								
								+ SHorizontalBox::Slot()
								.VAlign(VAlign_Center)
								.AutoWidth()
								[
									SNullWidget::NullWidget
								]
							]
						]
					]
				]
			]
		],
		InViewModel,
		InOwnerTableView);
}
