// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDerivedDataCacheSettings.h"

#include "Delegates/Delegate.h"
#include "Editor/EditorPerformanceSettings.h"
#include "HAL/Platform.h"
#include "Internationalization/Internationalization.h"
#include "Layout/Children.h"
#include "Layout/Margin.h"
#include "MessageLogModule.h"
#include "Misc/Attribute.h"
#include "Modules/ModuleManager.h"
#include "SlotBase.h"
#include "Styling/AppStyle.h"
#include "Styling/ISlateStyle.h"
#include "Styling/SlateColor.h"
#include "Types/SlateStructs.h"
#include "Types/WidgetActiveTimerDelegate.h"
#include "UObject/UObjectGlobals.h"
#include "Widgets/Images/SThrobber.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Text/STextBlock.h"

class SWidget;

#define LOCTEXT_NAMESPACE "DerivedDataEditor"

void SDerivedDataCacheSettingsDialog::Construct(const FArguments& InArgs)
{
	FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");
	TSharedRef<class IMessageLogListing> MessageLogListing = MessageLogModule.GetLogListing(TEXT("Config"));

	this->ChildSlot
	[
		SNew(SBox)
		.WidthOverride(480.f)
		[
			SNew(SBorder)
			.Padding(10.f)
			.BorderImage(FAppStyle::Get().GetBrush("Brushes.Panel"))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				/*.Expose(GridSlot)
				[
					GetGridPanel()
				]*/

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.Padding(FMargin(0.0f, 0.0f, 16.0f, 0.0f))
					[
						SNew(SVerticalBox)
						+SVerticalBox::Slot()
						.Padding(FMargin(0.0f, 0.0f, 0.0f, 10.0f))
						.VAlign(VAlign_Center)
						.HAlign(HAlign_Left)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("Setting1Label", "Setting1"))
							.ToolTipText( LOCTEXT("Setting1Label_Tooltip", "Setting1") )
						]
						+SVerticalBox::Slot()
						.Padding(FMargin(0.0f, 0.0f, 0.0f, 10.0f))
						.VAlign(VAlign_Center)
						.HAlign(HAlign_Left)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("Setting2Label", "Setting2"))
							.ToolTipText( LOCTEXT("Setting2Label_Tooltip", "Setting2") )
						]	
						+SVerticalBox::Slot()
						.Padding(FMargin(0.0f, 0.0f, 0.0f, 10.0f))
						.HAlign(HAlign_Left)
						[
							SNew(SCheckBox)
							.ForegroundColor(FSlateColor::UseForeground())
							.IsChecked(this, &SDerivedDataCacheSettingsDialog::AreNotificationsEnabled)
							.OnCheckStateChanged(this, &SDerivedDataCacheSettingsDialog::OnNotifcationsEnabledCheckboxChanged)
						]
					]
					+SHorizontalBox::Slot()
					.FillWidth(2.0f)
					[
						SNew(SVerticalBox)
						+SVerticalBox::Slot()
						.VAlign(VAlign_Center)
						.HAlign(HAlign_Left)
						.Padding(FMargin(0.0f, 0.0f, 0.0f, 10.0f))
						[
							SNew(SEditableTextBox)
							.Text(this, &SDerivedDataCacheSettingsDialog::GetSetting1Text)
							.ToolTipText( LOCTEXT("Setting1Label_Tooltip", "Setting1") )
							.OnTextCommitted(this, &SDerivedDataCacheSettingsDialog::OnSetting2TextCommited)
							.OnTextChanged(this, &SDerivedDataCacheSettingsDialog::OnSetting2TextCommited, ETextCommit::Default)
						]
						+SVerticalBox::Slot()
						.Padding(FMargin(0.0f, 0.0f, 0.0f, 10.0f))
						.VAlign(VAlign_Center)
						.HAlign(HAlign_Left)
						[
							SNew(SEditableTextBox)
							.Text(this, &SDerivedDataCacheSettingsDialog::GetSetting2Text)
							.ToolTipText( LOCTEXT("Setting2Label_Tooltip", "Setting2") )
							.OnTextCommitted(this, &SDerivedDataCacheSettingsDialog::OnSetting2TextCommited)
							.OnTextChanged(this, &SDerivedDataCacheSettingsDialog::OnSetting2TextCommited, ETextCommit::Default)
						]
						+ SVerticalBox::Slot()
						.Padding(FMargin(0.0f, 0.0f, 0.0f, 10.0f))
						.VAlign(VAlign_Center)
						.HAlign(HAlign_Left)
						[
							SNew(STextBlock)
							.TextStyle(FAppStyle::Get(), "DialogButtonText")
							.Text(LOCTEXT("EnableDDCNotifications", "Enable Notifications"))
						]
					]
					
				]

				//+ SVerticalBox::Slot()
				//.VAlign(VAlign_Top)
				//.AutoHeight()
				//.Padding(0.0f)
				//[
				//	SNew(SExpandableArea)
				//	.BorderImage(FAppStyle::GetBrush("NoBorder"))
				//	.InitiallyCollapsed(true)
				//	.HeaderContent()
				//	[
				//		SNew(STextBlock)
				//		.Text(LOCTEXT("LogTitle", "Derived Data Cache Log"))
				//	]
				//	.BodyContent()
				//	[
				//		SNew(SBorder)
				//		.BorderImage(FAppStyle::Get().GetBrush("Brushes.Recessed"))
				//		.Padding(0)
				//		[
				//			SNew(SBox)
				//			.HeightOverride(250)
				//			[
				//				MessageLogModule.CreateLogListingWidget(MessageLogListing)
				//			]
				//		]
				//	]
				//]

				+SVerticalBox::Slot()
				.VAlign(VAlign_Bottom)
				.Padding(8.0f, 16.0f, 8.0f, 16.0f)
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Left)
					[
						SNew(SThrobber)
						.Visibility(this, &SDerivedDataCacheSettingsDialog::GetThrobberVisibility)
					]
					+SHorizontalBox::Slot()
					.FillWidth(1.0f)
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
  							.TextStyle( FAppStyle::Get(), "DialogButtonText" )
							.Text(LOCTEXT("AcceptSettings", "Accept Settings"))
							.OnClicked( this, &SDerivedDataCacheSettingsDialog::OnAcceptSettings )
							.IsEnabled( this, &SDerivedDataCacheSettingsDialog::IsAcceptSettingsEnabled )
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(FMargin(5.0f, 0.0f))
						[
							SNew(SButton)
							.VAlign(VAlign_Center)
  							.TextStyle( FAppStyle::Get(), "DialogButtonText" )
							.Text(LOCTEXT("RunWithoutCache", "Disable Derived Data Cache"))
							.OnClicked( this, &SDerivedDataCacheSettingsDialog::OnDisableDerivedDataCache )
							.IsEnabled( this, &SDerivedDataCacheSettingsDialog::IsDerivedDataCacheEnabled)
						]
					]
				]
			]
		]
	];

	RegisterActiveTimer(0.5f, FWidgetActiveTimerDelegate::CreateSP(this, &SDerivedDataCacheSettingsDialog::UpdateGridPanels));
}

EActiveTimerReturnType SDerivedDataCacheSettingsDialog::UpdateGridPanels(double InCurrentTime, float InDeltaTime)
{
	/*(*GridSlot)
	[
		GetGridPanel()
	];*/

	SlatePrepass(GetPrepassLayoutScaleMultiplier());

	return EActiveTimerReturnType::Continue;
}

TSharedRef<SWidget> SDerivedDataCacheSettingsDialog::GetGridPanel()
{
	TSharedRef<SGridPanel> Panel =
		SNew(SGridPanel);

	return Panel;
}


FText SDerivedDataCacheSettingsDialog::GetSetting1Text() const
{
	return LOCTEXT("Setting1Text", "Setting1");
}

void SDerivedDataCacheSettingsDialog::OnSetting1TextCommited(const FText& InText, ETextCommit::Type InCommitType) const
{
	
}


FText SDerivedDataCacheSettingsDialog::GetSetting2Text() const
{
	return LOCTEXT("Setting2Text", "Setting2");
}

void SDerivedDataCacheSettingsDialog::OnSetting2TextCommited(const FText& InText, ETextCommit::Type InCommitType) const
{

}


EVisibility SDerivedDataCacheSettingsDialog::GetThrobberVisibility() const
{
	return EVisibility::Hidden;
}

bool SDerivedDataCacheSettingsDialog::IsDerivedDataCacheEnabled() const
{
	return true;
}

bool SDerivedDataCacheSettingsDialog::IsAcceptSettingsEnabled() const
{
	return true;
}

FReply SDerivedDataCacheSettingsDialog::OnAcceptSettings()
{

	return FReply::Handled();
}

FReply SDerivedDataCacheSettingsDialog::OnDisableDerivedDataCache()
{

	return FReply::Handled();
}

ECheckBoxState SDerivedDataCacheSettingsDialog::AreNotificationsEnabled() const
{	
	return GetDefault<UEditorPerformanceSettings>()->bEnableSharedDDCPerformanceNotifications ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SDerivedDataCacheSettingsDialog::OnNotifcationsEnabledCheckboxChanged(ECheckBoxState NewCheckboxState)
{
	UEditorPerformanceSettings* Settings = GetMutableDefault<UEditorPerformanceSettings>();
	Settings->bEnableSharedDDCPerformanceNotifications = NewCheckboxState == ECheckBoxState::Checked;
	Settings->PostEditChange();
}

#undef LOCTEXT_NAMESPACE