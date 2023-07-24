// Copyright Epic Games, Inc. All Rights Reserved.

#include "TranslationPickerEditWindow.h"

#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformCrt.h"
#include "ILocalizationServiceModule.h"
#include "ILocalizationServiceProvider.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/TextKey.h"
#include "Internationalization/TextLocalizationManager.h"
#include "Internationalization/TextNamespaceUtil.h"
#include "Layout/BasicLayoutWidgetSlot.h"
#include "Layout/Children.h"
#include "Layout/Margin.h"
#include "Layout/Visibility.h"
#include "Misc/Attribute.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "SlotBase.h"
#include "Styling/AppStyle.h"
#include "Styling/SlateTypes.h"
#include "TranslationDataManager.h"
#include "TranslationUnit.h"
#include "Types/SlateEnums.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

struct FGeometry;

#define LOCTEXT_NAMESPACE "TranslationPicker"

TSharedPtr<FTranslationPickerSettingsManager> FTranslationPickerSettingsManager::TranslationPickerSettingsManagerInstance;

// Default dimensions of the Translation Picker edit window (floating window also uses these sizes, so it matches roughly)
const int32 STranslationPickerEditWindow::DefaultEditWindowWidth = 500;
const int32 STranslationPickerEditWindow::DefaultEditWindowHeight = 500;

UTranslationPickerSettings::UTranslationPickerSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

void STranslationPickerEditWindow::Construct(const FArguments& InArgs)
{
	ParentWindow = InArgs._ParentWindow;
	PickedTexts = InArgs._PickedTexts;
	WindowContents = SNew(SBox);
	TSharedRef<SVerticalBox> TextsBox = SNew(SVerticalBox);
	UTranslationPickerSettings* TranslationPickerSettings = FTranslationPickerSettingsManager::Get()->GetSettings();

	bool bShowLocServiceCheckbox = ILocalizationServiceModule::Get().GetProvider().IsEnabled();

	if (!FParse::Param(FCommandLine::Get(), TEXT("AllowTranslationPickerSubmissionsToOneSky")))
	{
		bShowLocServiceCheckbox = false;
		TranslationPickerSettings->bSubmitTranslationPickerChangesToLocalizationService = false;
	}

	// Add a new Translation Picker Edit Widget for each picked text
	for (FText PickedText : PickedTexts)
	{
		TSharedPtr<SEditableTextBox> TextBox;

		TSharedRef<STranslationPickerEditWidget> NewEditWidget = 
			SNew(STranslationPickerEditWidget)
			.PickedText(PickedText)
			.bAllowEditing(true);

		EditWidgets.Add(NewEditWidget);

		TextsBox->AddSlot()
			.AutoHeight()
			.Padding(FMargin(5))
			[
				SNew(SBorder)
				[
					NewEditWidget
				]
			];
	}

	TSharedPtr<SEditableTextBox> TextBox;
	float DefaultPadding = 0.0f;

	// Layout the Translation Picker Edit Widgets and some save/close buttons below them
	WindowContents->SetContent(
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		[
			SNew(SVerticalBox)

			+SVerticalBox::Slot()
			[
				SNew(SScrollBox)
				+SScrollBox::Slot()
				.Padding(FMargin(8, 5, 8, 5))
				[
					TextsBox
				]
			]
			
			+SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Fill)
			.Padding(DefaultPadding)
			[
				SNew(SVerticalBox)
				
				+SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Left)
				.Padding(DefaultPadding)
				[
					SNew(SHorizontalBox)
					.Visibility(bShowLocServiceCheckbox ? EVisibility::Visible : EVisibility::Collapsed)
					
					+SHorizontalBox::Slot()
					.Padding(FMargin(3, 3, 3, 3))
					.VAlign(VAlign_Center)
					.AutoWidth()
					[
						SNew(SCheckBox)
						.HAlign(HAlign_Center)
						.IsChecked(TranslationPickerSettings->bSubmitTranslationPickerChangesToLocalizationService ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
						.ToolTipText(LOCTEXT("SubmitTranslationPickerChangesToLocalizationServiceToolTip", "Submit changes to localization service"))
						.OnCheckStateChanged_Lambda([&](ECheckBoxState CheckedState)
						{
							UTranslationPickerSettings* TranslationPickerSettingsLocal = FTranslationPickerSettingsManager::Get()->GetSettings();
							TranslationPickerSettingsLocal->bSubmitTranslationPickerChangesToLocalizationService = CheckedState == ECheckBoxState::Checked;
							TranslationPickerSettingsLocal->SaveConfig();
						}
						)
					]
					
					+SHorizontalBox::Slot()
					.Padding(FMargin(0, 0, 3, 0))
					.VAlign(VAlign_Center)
					.AutoWidth()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("SubmitTranslationPickerChangesToLocalizationService", "Save to Localization Service"))
						.ToolTipText(LOCTEXT("SubmitTranslationPickerChangesToLocalizationServiceToolTip", "Submit changes to localization service"))
					]
				]
				
				+SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Right)
				.Padding(FMargin(0, 5))
				[
					SNew(SUniformGridPanel)
					.SlotPadding(FAppStyle::GetMargin("StandardDialog.SlotPadding"))
					.MinDesiredSlotWidth(FAppStyle::GetFloat("StandardDialog.MinDesiredSlotWidth"))
					.MinDesiredSlotHeight(FAppStyle::GetFloat("StandardDialog.MinDesiredSlotHeight"))
					
					+SUniformGridPanel::Slot(0, 0)
					[
						SNew(SButton)
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						.ContentPadding(FAppStyle::GetMargin("StandardDialog.ContentPadding"))
						.OnClicked(this, &STranslationPickerEditWindow::SaveAllAndClose)
						.Text(LOCTEXT("SaveAllAndClose", "Save All and Close"))
					]
					
					+SUniformGridPanel::Slot(1, 0)
					[
						SNew(SButton)
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						.ContentPadding(FAppStyle::GetMargin("StandardDialog.ContentPadding"))
						.OnClicked(this, &STranslationPickerEditWindow::Close)
						.Text(LOCTEXT("CancelButton", "Cancel"))
					]
				]
			]
		]
	);

	ChildSlot
	[
		WindowContents.ToSharedRef()
	];
}

FReply STranslationPickerEditWindow::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		Close();
		
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

FReply STranslationPickerEditWindow::Close()
{
	if (ParentWindow.IsValid())
	{
		FSlateApplication::Get().RequestDestroyWindow(ParentWindow.Pin().ToSharedRef());
		ParentWindow.Reset();
	}

	return FReply::Handled();
}

FReply STranslationPickerEditWindow::SaveAllAndClose()
{
	TArray<UTranslationUnit*> TempArray;

	for (TSharedRef<STranslationPickerEditWidget> EditWidget : EditWidgets)
	{
		UTranslationUnit* TranslationUnit = EditWidget->GetTranslationUnitWithAnyChanges();
		if (TranslationUnit != nullptr && EditWidget->CanSave())
		{
			TempArray.Add(TranslationUnit);
		}
	}

	if (TempArray.Num() > 0)
	{
		UTranslationPickerSettings* TranslationPickerSettings = FTranslationPickerSettingsManager::Get()->GetSettings();
		// Save the data via translation data manager
		FTranslationDataManager::SaveSelectedTranslations(TempArray, ILocalizationServiceModule::Get().GetProvider().IsEnabled() && TranslationPickerSettings->bSubmitTranslationPickerChangesToLocalizationService);
	}

	Close();

	return FReply::Handled();
}

void STranslationPickerEditWidget::Construct(const FArguments& InArgs)
{
	PickedText = InArgs._PickedText;
	bAllowEditing = InArgs._bAllowEditing;

	// Try and get the localization information for this text
	FTextId TextId;
	FString SourceString;
	FString TranslationString;
	{
		if (const FString* SourceStringPtr = FTextInspector::GetSourceString(PickedText))
		{
			SourceString = *SourceStringPtr;
		}
		TranslationString = FTextInspector::GetDisplayString(PickedText);
		TextId = FTextInspector::GetTextId(PickedText);
	}

	// Try and find the LocRes the active translation came from
	// We assume the LocRes is named the same as the localization target
	FString LocResPath;
	FString LocTargetName;
	FString LocResCultureName;
	if (!TextId.IsEmpty() && FTextLocalizationManager::Get().GetLocResID(TextId.GetNamespace(), TextId.GetKey(), LocResPath))
	{
		LocTargetName = FPaths::GetBaseFilename(LocResPath);

		const FString CultureFilePath = FPaths::GetPath(LocResPath);
		LocResCultureName = FPaths::GetBaseFilename(CultureFilePath);
	}

	// Clean the package localization ID from the namespace (to mirror what the text gatherer does when scraping for translation data)
	FString CleanNamespace = TextNamespaceUtil::StripPackageNamespace(TextId.GetNamespace().GetChars());

	// Save the necessary data in UTranslationUnit for later.  This is what we pass to TranslationDataManager to save our edits
	TranslationUnit = NewObject<UTranslationUnit>();
	TranslationUnit->Namespace = CleanNamespace;
	TranslationUnit->Key = TextId.GetKey().GetChars();
	TranslationUnit->Source = SourceString;
	TranslationUnit->Translation = TranslationString;
	TranslationUnit->LocresPath = LocResPath;

	// Can only save if we have have an identity and are in a known localization target file
	bHasRequiredLocalizationInfoForSaving = !TextId.IsEmpty() && !LocTargetName.IsEmpty();

	TSharedPtr<SGridPanel> GridPanel;

	// Layout all our data
	ChildSlot
	[
		SNew(SHorizontalBox)
		
		+SHorizontalBox::Slot()
		.FillWidth(1)
		.Padding(FMargin(5))
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			[
				SAssignNew(GridPanel, SGridPanel)
				.FillColumn(1,1)
				
				+SGridPanel::Slot(0,0)
				.Padding(FMargin(2.5))
				.HAlign(HAlign_Right)
				[
					SNew(STextBlock)
					.TextStyle(FAppStyle::Get(), "RichTextBlock.Bold")
					.Text(LOCTEXT("SourceLabel", "Source:"))
				]

				+SGridPanel::Slot(0, 1)
				.Padding(FMargin(2.5))
				.HAlign(HAlign_Right)
				[
					SNew(SBox)
					// Hide translation if we don't have necessary information to modify, and is same as source
					.Visibility(!bHasRequiredLocalizationInfoForSaving && SourceString.Equals(TranslationString) ? EVisibility::Collapsed : EVisibility::Visible)
					[
						SNew(STextBlock)
						.TextStyle(FAppStyle::Get(), "RichTextBlock.Bold")
						.Text(FText::Format(LOCTEXT("TranslationLabel", "Translation ({0}):"), FText::AsCultureInvariant(LocResCultureName)))
					]
				]
				
				+SGridPanel::Slot(1, 0)
				.Padding(FMargin(2.5))
				[
					SNew(SMultiLineEditableTextBox)
					.IsReadOnly(true)
					.Text(FText::AsCultureInvariant(SourceString))
				]

				+SGridPanel::Slot(1, 1)
				.Padding(FMargin(2.5))
				[
					SNew(SBox)
					// Hide translation if we don't have necessary information to modify, and is same as source
					.Visibility(!bHasRequiredLocalizationInfoForSaving && SourceString.Equals(TranslationString) ? EVisibility::Collapsed : EVisibility::Visible)
					[
						SAssignNew(TextBox, SMultiLineEditableTextBox)
						.IsReadOnly(!bAllowEditing || !bHasRequiredLocalizationInfoForSaving)
						.Text(FText::AsCultureInvariant(TranslationString))
						.HintText(LOCTEXT("TranslationEditTextBox_HintText", "Enter/edit translation here."))
					]
				]
			]
		]
	];

	if (!TextId.IsEmpty())
	{
		GridPanel->AddSlot(0, 2)
			.Padding(FMargin(2.5))
			.HAlign(HAlign_Right)
			[
				SNew(STextBlock)
				.TextStyle(FAppStyle::Get(), "RichTextBlock.Bold")
				.Text(LOCTEXT("NamespaceLabel", "Namespace:"))
			];
		GridPanel->AddSlot(1, 2)
			.Padding(FMargin(2.5))
			[
				SNew(SEditableTextBox)
				.IsReadOnly(true)
				.Text(FText::AsCultureInvariant(CleanNamespace))
			];
		GridPanel->AddSlot(0, 3)
			.Padding(FMargin(2.5))
			.HAlign(HAlign_Right)
			[
				SNew(STextBlock)
				.TextStyle(FAppStyle::Get(), "RichTextBlock.Bold")
				.Text(LOCTEXT("KeyLabel", "Key:"))
			];
		GridPanel->AddSlot(1, 3)
			.Padding(FMargin(2.5))
			[
				SNew(SEditableTextBox)
				.IsReadOnly(true)
				.Text(FText::AsCultureInvariant(TextId.GetKey().GetChars()))
			];
		
		if (bHasRequiredLocalizationInfoForSaving)
		{
			GridPanel->AddSlot(0, 4)
				.Padding(FMargin(2.5))
				.HAlign(HAlign_Right)
				[
					SNew(STextBlock)
					.TextStyle(FAppStyle::Get(), "RichTextBlock.Bold")
					.Text(LOCTEXT("LocresFileLabel", "Target:"))
				];
			GridPanel->AddSlot(1, 4)
				.Padding(FMargin(2.5))
				[
					SNew(SEditableTextBox)
					.IsReadOnly(true)
					.Text(FText::AsCultureInvariant(LocTargetName))
				];
			GridPanel->AddSlot(0, 5)
				.Padding(FMargin(2.5))
				.ColumnSpan(2)
				.HAlign(HAlign_Right)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					.ContentPadding(FAppStyle::GetMargin("StandardDialog.ContentPadding"))
					.OnClicked(this, &STranslationPickerEditWidget::SaveAndPreview)
					.IsEnabled(bHasRequiredLocalizationInfoForSaving)
					.Visibility(bAllowEditing ? EVisibility::Visible : EVisibility::Collapsed)
					.Text(bHasRequiredLocalizationInfoForSaving ? LOCTEXT("SaveAndPreviewButtonText", "Save and Preview") : LOCTEXT("SaveAndPreviewButtonDisabledText", "Cannot Save"))
				];
		}
		else
		{
			GridPanel->AddSlot(0, 4)
				.Padding(FMargin(2.5))
				.ColumnSpan(2)
				.HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("TextLocalizable_RequiresGather", "This text is localizable (requires gather)."))
				];
		}
	}
	else
	{
		FText TextNotLocalizableReason = LOCTEXT("TextNotLocalizable_Generic", "This text is not localizable.");
		if (PickedText.IsCultureInvariant())
		{
			TextNotLocalizableReason = LOCTEXT("TextNotLocalizable_CultureInvariant", "This text is not localizable (culture-invariant).");
		}
		else if (PickedText.IsTransient())
		{
			TextNotLocalizableReason = LOCTEXT("TextNotLocalizable_Transient", "This text is not localizable (transient).");
		}
		else if (!PickedText.ShouldGatherForLocalization())
		{
			TextNotLocalizableReason = LOCTEXT("TextNotLocalizable_InvalidForGather", "This text is not localizable (invalid for gather).");
		}

		GridPanel->AddSlot(0, 2)
			.Padding(FMargin(2.5))
			.ColumnSpan(2)
			.HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Text(TextNotLocalizableReason)
			];
	}
}

void STranslationPickerEditWidget::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(TranslationUnit);
}

FReply STranslationPickerEditWidget::SaveAndPreview()
{
	// Update translation string from entered text
	TranslationUnit->Translation = TextBox->GetText().ToString();
	UTranslationPickerSettings* TranslationPickerSettings = FTranslationPickerSettingsManager::Get()->GetSettings();

	// Save the data via translation data manager
	TArray<UTranslationUnit*> TempArray;
	TempArray.Add(TranslationUnit);
	FTranslationDataManager::SaveSelectedTranslations(TempArray, ILocalizationServiceModule::Get().GetProvider().IsEnabled() && TranslationPickerSettings->bSubmitTranslationPickerChangesToLocalizationService);

	return FReply::Handled();
}

UTranslationUnit* STranslationPickerEditWidget::GetTranslationUnitWithAnyChanges()
{
	if (TranslationUnit)
	{
		// Update translation string from entered text
		TranslationUnit->Translation = TextBox->GetText().ToString();

		return TranslationUnit;
	}

	return nullptr;
}

#undef LOCTEXT_NAMESPACE
