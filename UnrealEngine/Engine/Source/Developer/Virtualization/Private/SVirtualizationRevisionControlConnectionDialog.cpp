// Copyright Epic Games, Inc. All Rights Reserved.

#include "SVirtualizationRevisionControlConnectionDialog.h"

#if UE_VA_WITH_SLATE

#include "Async/ManualResetEvent.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/App.h"
#include "Misc/ConfigCacheIni.h"
#include "SourceControlHelpers.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "Virtualization"

namespace UE::Virtualization
{

SRevisionControlConnectionDialog::FResult SRevisionControlConnectionDialog::RunDialog(FStringView CurrentPort, FStringView CurrentUsername)
{
	if (FApp::IsUnattended())
	{
		UE_LOG(LogVirtualization, Warning, TEXT("Skipping attempt to show SRevisionControlConnectionDialog as the application is unattended"));
		return FResult();
	}

	if (!IsInGameThread())
	{
		UE_LOG(LogVirtualization, Warning, TEXT("Attempting to show SRevisionControlConnectionDialog on a worker thread!"));
		return FResult();
	}

	if (!FSlateApplication::IsInitialized() || FSlateApplication::Get().GetRenderer() == nullptr)
	{
		UE_LOG(LogVirtualization, Warning, TEXT("Attempting to show SRevisionControlConnectionDialog before slate is initialized"));
		return FResult();
	}

	UE_LOG(LogVirtualization, Display, TEXT("Creating dialog"));

	TSharedPtr<SWindow> DialogWindow = SNew(SWindow)
		.Title(LOCTEXT("VASCSettings", "Perforce Source Control Backend Settings"))
		.FocusWhenFirstShown(true)
		.SupportsMinimize(false)
		.SupportsMaximize(false)
		.SizingRule(ESizingRule::Autosized)
		.HasCloseButton(false);

	TSharedPtr<SRevisionControlConnectionDialog> DialogWidget;

	TSharedPtr<SBorder> DialogWrapper =
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		.Padding(4.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(16.0f, 16.0f, 16.0f, 0.0f)
			[
				SAssignNew(DialogWidget, SRevisionControlConnectionDialog, CurrentPort, CurrentUsername)
				.Window(DialogWindow)
			]
		];

	DialogWindow->SetContent(DialogWrapper.ToSharedRef());

	UE_LOG(LogVirtualization, Display, TEXT("Connection to source control for virtualized assets failed. Offering the user the choice to retry or continue anyway"));

	TSharedPtr<SWindow> ParentWindow = FSlateApplication::Get().GetActiveTopLevelWindow();
	FSlateApplication::Get().AddModalWindow(DialogWindow.ToSharedRef(), ParentWindow);

	if (DialogWidget->GetResult() == SRevisionControlConnectionDialog::EResult::Retry)
	{
		return FResult(DialogWidget->GetPort(), DialogWidget->GetUserName());
	}

	return FResult();
}

void SRevisionControlConnectionDialog::Construct(const FArguments& InArgs, FStringView CurrentPort, FStringView CurrentUsername)
{
	WindowWidget = InArgs._Window;

	const FString CurPort = TEXT("<P4PORT Here>");
	const FString CurUser = TEXT("<P4USER Here>");

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.Padding(FMargin(0.0f, 0.0f, 16.0f, 16.0f))
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(FMargin(0.0f, 0.0f, 8.0f, 0.0f))
			.HAlign(HAlign_Left)
			.AutoWidth()
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.Padding(FMargin(0.0f, 0.0f, 0.0f, 0.0f))
				.AutoHeight()
				[
					SNew(SImage)
					.Image(FAppStyle::GetBrush("Icons.Error"))
				]
			]
			+ SHorizontalBox::Slot()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("VASCMsg", "Failed to connect to the source control backend.\nThis may prevent you from accessing virtualized data in the future.\n\nPlease enter the correct source control settings below."))
				.AutoWrapText(true)
			]
		]
		+ SVerticalBox::Slot()
		.Padding(FMargin(0.0f, 0.0f, 16.0f, 16.0f))
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.Padding(FMargin(0.0f, 0.0f, 16.0f, 0.0f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.Padding(FMargin(0.0f, 0.0f, 0.0f, 10.0f))
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Right)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("PortLabel", "Server"))
					.ToolTipText(LOCTEXT("PortLabel_Tooltip", "The server and port for your Perforce server. Usage ServerName:1234."))
				]
				+ SVerticalBox::Slot()
				.Padding(FMargin(0.0f, 0.0f, 0.0f, 10.0f))
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Right)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("UserNameLabel", "User Name"))
					.ToolTipText(LOCTEXT("UserNameLabel_Tooltip", "Perforce username."))
				]
			]
			+ SHorizontalBox::Slot()
			.FillWidth(2.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.VAlign(VAlign_Center)
				.Padding(FMargin(0.0f, 0.0f, 0.0f, 10.0f))
				[
					SAssignNew(PortTextWidget, SEditableTextBox)
					.Text(FText::FromString(FString(CurrentPort)))
					.ToolTipText(LOCTEXT("VASC_PortTip", "The server and port for your Perforce server. Usage ServerName:1234."))
				]
				+ SVerticalBox::Slot()
				.Padding(FMargin(0.0f, 0.0f, 0.0f, 10.0f))
				.VAlign(VAlign_Center)
				[
					SAssignNew(UsernameTextWidget, SEditableTextBox)
					.Text(FText::FromString(FString(CurrentUsername)))
					.ToolTipText(LOCTEXT("VASC_UserTip", "Perforce username."))
				]
			]
		]
		+ SVerticalBox::Slot()
		.Padding(FMargin(0.0f, 0.0f, 16.0f, 16.0f))
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(FMargin(0.0f, 0.0f, 8.0f, 0.0f))
			.HAlign(HAlign_Left)
			.AutoWidth()
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.Padding(FMargin(0.0f, 0.0f, 0.0f, 0.0f))
				.AutoHeight()
				[
					SNew(SImage)
					.Image(FAppStyle::GetBrush("Icons.Warning"))
				]
			]
			+ SHorizontalBox::Slot()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("VASCSkipWarning", "Skipping may cause future editor instability if virtualized data is required!"))
				.AutoWrapText(true)
			]
		]
		+ SVerticalBox::Slot()
		.Padding(FMargin(0.0f, 0.0f, 16.0f, 16.0f))
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			[
				SNew(SButton)
				.VAlign(VAlign_Center)
				.TextStyle(FAppStyle::Get(), "DialogButtonText")
				.Text(LOCTEXT("VASC_Reset", "Reset To Defaults"))
				.OnClicked(this, &SRevisionControlConnectionDialog::OnResetToDefaults)
			]
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Right)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(FMargin(5.0f, 0.0f))
				[
					SNew(SButton)
					.VAlign(VAlign_Center)
					.ButtonStyle(FAppStyle::Get(), "PrimaryButton")
					.TextStyle(FAppStyle::Get(), "DialogButtonText")
					.Text(LOCTEXT("VASC_Retry", "Retry Connection"))
					.OnClicked(this, &SRevisionControlConnectionDialog::OnRetryConnection)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(FMargin(5.0f, 0.0f))
				[
					SNew(SButton)
					.VAlign(VAlign_Center)
					.TextStyle(FAppStyle::Get(), "DialogButtonText")
					.Text(LOCTEXT("VASC_Skip", "Skip"))
					.OnClicked(this, &SRevisionControlConnectionDialog::OnSkip)
				]
			]
		]
	];
}

void SRevisionControlConnectionDialog::CloseModalDialog()
{
	if (WindowWidget.IsValid())
	{
		WindowWidget.Pin()->RequestDestroyWindow();
	}
}

FReply SRevisionControlConnectionDialog::OnResetToDefaults()
{
	GConfig->EmptySection(TEXT("PerforceSourceControl.VirtualizationSettings"), SourceControlHelpers::GetSettingsIni());

	Port.Empty();
	UserName.Empty();

	CloseModalDialog();

	return FReply::Handled();
}

FReply SRevisionControlConnectionDialog::OnRetryConnection()
{
	UE_LOG(LogVirtualization, Display, TEXT("User opted to retry connecting to source control"));

	Result = EResult::Retry;

	Port = PortTextWidget->GetText().ToString();
	UserName = UsernameTextWidget->GetText().ToString();

	CloseModalDialog();

	return FReply::Handled();
}

FReply SRevisionControlConnectionDialog::OnSkip()
{
	UE_LOG(LogVirtualization, Warning, TEXT("User opted not to connect to source control. Virtualized data may not be accessible!"));

	Result = EResult::Skip;

	CloseModalDialog();

	return FReply::Handled();
}

static FAutoConsoleCommand CCmdTestDialog = FAutoConsoleCommand(
	TEXT("TestVADialog"),
	TEXT(""),
	FConsoleCommandDelegate::CreateStatic(SRevisionControlConnectionDialog::RunDialogCvar));

} // namespace UE::Virtualization

#undef LOCTEXT_NAMESPACE

#endif //UE_VA_WITH_SLATE
