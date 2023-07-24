// Copyright Epic Games, Inc. All Rights Reserved.

#include "GLTFOptionsWindow.h"

#include "DetailsViewArgs.h"
#include "IDetailsView.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"

#define LOCTEXT_NAMESPACE "GLTFOptionsWindow"

void SGLTFOptionsWindow::Construct(const FArguments& InArgs)
{
	ImportOptions = InArgs._ImportOptions;
	Window = InArgs._WidgetWindow;
	bShouldImport = false;

	TSharedPtr<SBox> DetailsViewBox;
	const FText VersionText(FText::Format(LOCTEXT("GLTFOptionWindow_Version", " Version   {0}"), FText::FromString("1.0")));

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.Padding(0.0f, 10.0f)
		.AutoHeight()
		[
			SNew(SInlineEditableTextBlock)
			.IsReadOnly(true)
			.Text(InArgs._FileNameText)
			.ToolTipText(InArgs._FilePathText)
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SInlineEditableTextBlock)
			.IsReadOnly(true)
			.Text(InArgs._PackagePathText)
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(2)
		[
			SAssignNew(DetailsViewBox, SBox)
			.MinDesiredHeight(320.0f)
			.MinDesiredWidth(450.0f)
		]
		+ SVerticalBox::Slot()
		.MaxHeight(50)
		[
			SNew(SUniformGridPanel)
			.SlotPadding(5)
			+ SUniformGridPanel::Slot(0, 0)
			.HAlign(HAlign_Left)
			[
				SNew(SUniformGridPanel)
				.SlotPadding(5)
				+ SUniformGridPanel::Slot(0, 0)
				.HAlign(HAlign_Left)
				[
					SNew(SInlineEditableTextBlock)
					.IsReadOnly(true)
					.Text(VersionText)
				]
			]
			+ SUniformGridPanel::Slot(1, 0)
			.HAlign(HAlign_Right)
			[
				SNew(SUniformGridPanel)
				.SlotPadding(5)
				+ SUniformGridPanel::Slot(0, 0)
				.HAlign(HAlign_Right)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					.Text(LOCTEXT("GLTFOptionWindow_ImportMaterials", "Import"))
					.ToolTipText(LOCTEXT("GLTFOptionWindow_ImportMaterials_ToolTip", "Import the file and add to the current Level"))
					.OnClicked(this, &SGLTFOptionsWindow::OnImport)
				]
				+ SUniformGridPanel::Slot(1, 0)
				.HAlign(HAlign_Right)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					.Text(LOCTEXT("GLTFOptionWindow_Cancel", "Cancel"))
					.ToolTipText(LOCTEXT("GLTFOptionWindow_Cancel_ToolTip", "Cancel importing this file"))
					.OnClicked(this, &SGLTFOptionsWindow::OnCancel)
				]
			]
		]
	];

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bAllowSearch = false;
	DetailsViewArgs.bAllowMultipleTopLevelObjects = true;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;

	TSharedPtr<IDetailsView> DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
	DetailsViewBox->SetContent(DetailsView.ToSharedRef());
	DetailsView->SetObject(ImportOptions);
}

FReply SGLTFOptionsWindow::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) /*override*/
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		return OnCancel();
	}

	return FReply::Unhandled();
}

FReply SGLTFOptionsWindow::OnImport()
{
	bShouldImport = true;
	if (Window.IsValid())
	{
		Window.Pin()->RequestDestroyWindow();
	}
	return FReply::Handled();
}

FReply SGLTFOptionsWindow::OnCancel()
{
	bShouldImport = false;
	if (Window.IsValid())
	{
		Window.Pin()->RequestDestroyWindow();
	}
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
