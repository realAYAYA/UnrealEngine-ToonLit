// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/SMoviePipelineConfigEditor.h"
#include "Widgets/MoviePipelineWidgetConstants.h"
#include "Widgets/SMoviePipelineConfigSettings.h"
#include "MoviePipelineShotConfig.h"
#include "MoviePipelineSetting.h"
#include "MoviePipelineQueue.h"
#include "MoviePipelineUtils.h"

// Core includes
#include "UObject/UObjectIterator.h"
#include "Templates/SubclassOf.h"
#include "ClassIconFinder.h"

// AssetRegistry includes
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/AssetData.h"
 
// ContentBrowser includes
#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"
 
// AssetTools includes
#include "AssetToolsModule.h"
 
// Slate includes
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Text/SMultiLineEditableText.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Views/SListView.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Styling/SlateIconFinder.h"
#include "SPositiveActionButton.h"


// EditorStyle includes
#include "Styling/AppStyle.h"
#include "EditorFontGlyphs.h"
#include "ScopedTransaction.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "MovieRenderPipelineStyle.h"
#include "FrameNumberDetailsCustomization.h"
#include "IDetailCustomization.h"
#include "Editor.h"

#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IPropertyUtilities.h"

#include "Customizations/ConsoleVariableCustomization.h"
#include "Customizations/ConsoleVariableSettingCustomization.h"
#include "MoviePipelineConsoleVariableSetting.h"
#include "MoviePipelineOutputSetting.h"

#define LOCTEXT_NAMESPACE "SMoviePipelineEditor"




/** This is a specific auto-complete widget made for the MoviePipeline that isn't very flexible. It's
* similar to SSuggestionTextBox, but SSuggestionTextBox doesn't handle more than one word/suggestions
* mid string. This widget is hardcoded to look for '{' characters (for {format_tokens}) and then auto
* completes them from a list and fixes up the {} braces.
*/
class SMoviePipelineAutoCompleteTextBox : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SMoviePipelineAutoCompleteTextBox)
		: _Text()
		, _Suggestions()
	{}

	SLATE_ATTRIBUTE(FText, Text)
	SLATE_ATTRIBUTE(TArray<FString>, Suggestions)
	/** Called whenever the text is changed programmatically or interactively by the user. */
	SLATE_EVENT(FOnTextChanged, OnTextChanged)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		ChildSlot
		[
			SAssignNew(MenuAnchor, SMenuAnchor)
			.Placement(MenuPlacement_ComboBox)
			[
				SAssignNew(TextBox, SMultiLineEditableTextBox)
				.Text(InArgs._Text)
				.OnKeyDownHandler(this, &SMoviePipelineAutoCompleteTextBox::OnKeyDown)
				.OnTextChanged(this, &SMoviePipelineAutoCompleteTextBox::HandleTextBoxTextChanged)
				.SelectWordOnMouseDoubleClick(true)
				.AllowMultiLine(false)
			]
			.MenuContent
			(
				SNew(SBorder)
				.Padding(FMargin(2))
				[
					SAssignNew(VerticalBox, SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SAssignNew(SuggestionListView, SListView<TSharedPtr<FString>>)
						.ItemHeight(18.f)
						.ListItemsSource(&Suggestions)
						.SelectionMode(ESelectionMode::Single)
						.OnGenerateRow(this, &SMoviePipelineAutoCompleteTextBox::HandleSuggestionListViewGenerateRow)
						.OnSelectionChanged(this, &SMoviePipelineAutoCompleteTextBox::HandleSuggestionListViewSelectionChanged)
					]
				]
			)
		];

		// We just call it once and cache it for now as the selection code isn't tested against
		// the amount of suggestions changing.
		AllSuggestions.Append(InArgs._Suggestions.Get());
		OnTextChanged = InArgs._OnTextChanged;
	}

	FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& KeyEvent)
	{
		if (MenuAnchor->IsOpen())
		{
			if (KeyEvent.GetKey() == EKeys::Up)
			{
				// Because the pop-up dialog is below the text, 'up' actually goes to an earlier item in the list.
				int32 NewSuggestionIndex = CurrentSuggestionIndex - 1;
				if (NewSuggestionIndex < 0)
				{
					NewSuggestionIndex = Suggestions.Num() - 1;
				}

				SetActiveSuggestionIndex(NewSuggestionIndex);
				return FReply::Handled();
			}
			else if(KeyEvent.GetKey() == EKeys::Down)
			{
				int32 NewSuggestionIndex = CurrentSuggestionIndex + 1;
				if (NewSuggestionIndex > Suggestions.Num() - 1)
				{
					NewSuggestionIndex = 0;
				}

				SetActiveSuggestionIndex(NewSuggestionIndex);
				return FReply::Handled();
			}
			else if (KeyEvent.GetKey() == EKeys::Tab || KeyEvent.GetKey() == EKeys::Enter)
			{
				if(CurrentSuggestionIndex >= 0 && CurrentSuggestionIndex <= Suggestions.Num() - 1)
				{
					// Trigger the auto-complete for the highlighted suggestion
					FString SuggestionText = *Suggestions[CurrentSuggestionIndex];
					ReplaceRelevantTextWithSuggestion(SuggestionText);
					return FReply::Handled();
				}
			}
		}
		return FReply::Unhandled();
	}


	void FindAutoCompleteableTextAtPos(const FString& InWholeString, int32 InCursorPos, FString& OutStr, bool& bShowAutoComplete)
	{
		OutStr = FString();
		bShowAutoComplete = false;

		// We want to find a { brace on or to the left of InCursorPos, but if we find a } we
		// stop looking, because that's a brace for another text. (+1 for ::FromEnd off by one)
		int32 StartingBracePos = InWholeString.Find(TEXT("{"), ESearchCase::Type::IgnoreCase, ESearchDir::Type::FromEnd, InCursorPos + 1);
		int32 PreviousEndBracePos = InWholeString.Find(TEXT("}"), ESearchCase::Type::IgnoreCase, ESearchDir::Type::FromEnd, InCursorPos);

		if (StartingBracePos < PreviousEndBracePos)
		{
			return;
		}

		FString AutoCompleteText;

		// Now that we found a {, take the substring between it and either the next }, or the end of the string.
		int32 NextEndBracePos = InWholeString.Find(TEXT("}"), ESearchCase::Type::IgnoreCase, ESearchDir::Type::FromStart, InCursorPos);
		if (StartingBracePos >= 0)
		{
			int32 Count = InWholeString.Len() - StartingBracePos;
			if (NextEndBracePos >= 0)
			{
				Count = NextEndBracePos - StartingBracePos;
			}

			AutoCompleteText = InWholeString.Mid(StartingBracePos + 1, Count - 1);
		}

		OutStr = AutoCompleteText;
		bShowAutoComplete = StartingBracePos >= 0 && OutStr.Len() == 0;
	}

	void ReplaceRelevantTextWithSuggestion(const FString& InSuggestionText)
	{
		FString TextBoxText = TextBox->GetText().ToString();
		int32 CursorPos = TextBoxText.Len();
		FTextLocation CursorLoc = TextBox->GetCursorLocation();
		if (CursorLoc.IsValid())
		{
			CursorPos = FMath::Clamp(CursorLoc.GetOffset(), 0, CursorPos);
		}
		
		// Look for the { to the left of the cursor. We search StrPositionIndex from +1 here due to a bug in ::FromEnd being off by one.
		int32 StartingBracePos = TextBoxText.Find(TEXT("{"), ESearchCase::Type::IgnoreCase, ESearchDir::Type::FromEnd, CursorPos + 1);

		// Now that we found a {, take the substring between it and either the next }, or the end of the string.
		int32 NextEndBracePos = TextBoxText.Find(TEXT("}"), ESearchCase::Type::IgnoreCase, ESearchDir::Type::FromStart, CursorPos);
		int32 NewCursorPos = 0;
		if (StartingBracePos >= 0)	
		{
			// +1 to keep the left { brace
			FString Left = TextBoxText.Left(StartingBracePos+1);
			FString Right;

			if (NextEndBracePos >= 0)
			{
				Right = TextBoxText.RightChop(NextEndBracePos);
			}

			// Since the user chose the suggestion ensure there's already a } brace to close off the pair.
			if (!Right.StartsWith(TEXT("}")))
			{
				Right = FString::Printf(TEXT("}%s"), *Right);
			}

			TextBoxText = Left + InSuggestionText + Right;
			// We subtract 1 from the Right as we want to put the cursor after the automatically generated "}" token.
			NewCursorPos = TextBoxText.Len() - (Right.Len() - 1);
		}

		TextBox->SetText(FText::FromString(TextBoxText));
		TextBox->GoTo(FTextLocation(0, NewCursorPos));
	}

	void HandleTextBoxTextChanged(const FText& InText)
	{
		OnTextChanged.ExecuteIfBound(InText);

		FString TextAsStr = InText.ToString();
		if (TextAsStr.Len() > 0)
		{
			FString OutStr;
			bool bShowAutoComplete;

			int32 CursorPos = TextAsStr.Len();
			FTextLocation CursorLoc = TextBox->GetCursorLocation();
			if (CursorLoc.IsValid())
			{
				CursorPos = FMath::Clamp(CursorLoc.GetOffset(), 0, CursorPos);
			}

			FindAutoCompleteableTextAtPos(TextAsStr, CursorPos, OutStr, bShowAutoComplete);
			FilterVisibleSuggestions(OutStr, bShowAutoComplete);
		}	
		else
		{
			// If they have no text, suggest all possible solutions
			FilterVisibleSuggestions(FString(), false);
		}
	}

	void FilterVisibleSuggestions(const FString& StrToMatch, const bool bForceShowAll)
	{
		Suggestions.Reset();
		for (const FString& Suggestion : AllSuggestions)
		{
			if (Suggestion.Contains(StrToMatch) || bForceShowAll)
			{
				Suggestions.Add(MakeShared<FString>(Suggestion));
			}
		}

		if (Suggestions.Num() > 0)
		{
			// We don't focus the menu (because then you can't type on the keyboard) and instead
			// keep the focus on the text field and bubble the keyboard commands to it.
			const bool bIsOpen = true;
			const bool bFocusMenu = false;
			MenuAnchor->SetIsOpen(bIsOpen, bFocusMenu);
			SuggestionListView->RequestScrollIntoView(Suggestions[0]);
		}
		else
		{
			CloseMenuAndReset();
		}
	}

	void CloseMenuAndReset()
	{
		const bool bIsOpen = false;
		MenuAnchor->SetIsOpen(bIsOpen);

		// Reset their index when the drawer closes so that the first item is always selected when we re-open.
		CurrentSuggestionIndex = -1;
	}

	void SetActiveSuggestionIndex(int32 InIndex)
	{
		if (InIndex < 0 || InIndex >= Suggestions.Num())
		{
			return;
		}

		TSharedPtr<FString> Suggestion = Suggestions[InIndex];
		SuggestionListView->SetSelection(Suggestion);
		if (!SuggestionListView->IsItemVisible(Suggestion))
		{
			SuggestionListView->RequestScrollIntoView(Suggestion);
		}
		CurrentSuggestionIndex = InIndex;
	}

	TSharedRef<ITableRow> HandleSuggestionListViewGenerateRow(TSharedPtr<FString> Text, const TSharedRef<STableViewBase>& OwnerTable)
	{
		FString SuggestionText = *Text;

		return SNew(STableRow<TSharedPtr<FString> >, OwnerTable)
			[
				SNew(SBox)
				[
					SNew(STextBlock)
					.Text(FText::FromString(SuggestionText))
				]
			];
	}

	void HandleSuggestionListViewSelectionChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo)
	{
		// This is called when clicking on an item and when navigating the menu via arrow keys, but we already
		// handle arrow keys in OnKeyDown so we only want to handle mouse click here.
		if (SelectInfo == ESelectInfo::Type::OnMouseClick)
		{
			// Trigger the auto-complete for the highlighted suggestion
			ReplaceRelevantTextWithSuggestion(*NewValue);
			CloseMenuAndReset();
		}
	}

private:
	TSharedPtr<SListView<TSharedPtr<FString>>> SuggestionListView;
	TSharedPtr<SMultiLineEditableTextBox> TextBox;
	TSharedPtr<SMenuAnchor> MenuAnchor;
	TSharedPtr<SVerticalBox> VerticalBox;
	// Holds a delegate that is executed when the text has changed.
	FOnTextChanged OnTextChanged;

	// The pool of suggestions to show
	TArray<FString> AllSuggestions;
	// The currently filtered suggestion list
	TArray<TSharedPtr<FString>> Suggestions;
	int32 CurrentSuggestionIndex = -1;
};

class FOutputFormatDetailsCustomization : public IDetailCustomization
{
public:
	/** Creates a detail customization instance */
	static TSharedRef<IDetailCustomization> MakeInstance(TSharedRef<SMoviePipelineConfigEditor> InEditor)
	{
		return MakeShared<FOutputFormatDetailsCustomization>(InEditor);
	}

	FOutputFormatDetailsCustomization(TSharedRef<SMoviePipelineConfigEditor> InEditor)
	{
		OwningEditor = InEditor;
	}

protected:
	//~ Begin IDetailCustomization interface
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override
	{
		OutputFormatPropertyHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMoviePipelineOutputSetting, FileNameFormat));
		const bool bShowChildren = false;
		FText InitialText;
		OutputFormatPropertyHandle->GetValueAsDisplayText(InitialText);

		DetailBuilder.EditDefaultProperty(OutputFormatPropertyHandle)->CustomWidget(bShowChildren)
			.NameContent()
			[
				OutputFormatPropertyHandle->CreatePropertyNameWidget()
			]
			.ValueContent()
			.MinDesiredWidth(200.0f)
			[
				SNew(SMoviePipelineAutoCompleteTextBox)
				.Text(this, &FOutputFormatDetailsCustomization::GetText)
				.Suggestions(this, &FOutputFormatDetailsCustomization::GetSuggestions)
				.OnTextChanged(this, &FOutputFormatDetailsCustomization::OnTextChanged)
			];


	}
	//~ End IDetailCustomization interface

	TArray<FString> GetSuggestions() const
	{
		TSharedPtr<SMoviePipelineConfigEditor> ConfigEditor = OwningEditor.Pin();
		if (ConfigEditor.IsValid())
		{
			TArray<FString> Suggestions;
			if (ConfigEditor->CachedPipelineConfig.IsValid())
			{
				FMoviePipelineFormatArgs FormatArgs;
				FormatArgs.InJob = ConfigEditor->CachedOwningJob.Get();

				// Find the primary configuration that owns us (we won't necessairly have a job, but
				// we might be able to find the primary config that this setting is outered to.
				UMoviePipelinePrimaryConfig* PrimaryConfig = Cast<UMoviePipelinePrimaryConfig>(ConfigEditor->CachedPipelineConfig.Get());
				if (PrimaryConfig)
				{
					PrimaryConfig->GetFormatArguments(FormatArgs);
				}

				for (const TPair<FString, FString>& KVP : FormatArgs.FilenameArguments)
				{
					Suggestions.Add(KVP.Key);
				}
			}

			return Suggestions;
		}

		return TArray<FString>();
	}

	void OnTextChanged(const FText& InValue)
	{
		OutputFormatPropertyHandle->SetValue(InValue.ToString());
	}

	FText GetText() const
	{
		FText DisplayText;
		OutputFormatPropertyHandle->GetValueAsDisplayText(DisplayText);
		return DisplayText;
	}

	TSharedPtr<IPropertyHandle> OutputFormatPropertyHandle;
	TWeakPtr<SMoviePipelineConfigEditor> OwningEditor;
};

/**
 * Widget used to edit a Movie Render Pipeline Shot Config.
 */
UE_DISABLE_OPTIMIZATION_SHIP
void SMoviePipelineConfigEditor::Construct(const FArguments& InArgs)
{
	bRequestDetailsRefresh = true;
	PipelineConfigAttribute = InArgs._PipelineConfig;
	OwningJobAttribute = InArgs._OwningJob;
    
	DetailsBox = SNew(SScrollBox);
	DetailsBox->SetScrollBarRightClickDragAllowed(true);
     
	SettingsWidget = SNew(SMoviePipelineConfigSettings)
	.OnSelectionChanged(this, &SMoviePipelineConfigEditor::OnSettingsSelectionChanged);
    
	CheckForNewSettingsObject();
     
	// Automatically try to select the Output setting as it's the most commonly edited one.
	// If that fails, we fall back to the first setting.
	{
		UMoviePipelineOutputSetting* OutputSetting = CachedPipelineConfig->FindSetting<UMoviePipelineOutputSetting>();
		if (OutputSetting)
		{
			TArray<UMoviePipelineSetting*> SelectedSettings;
			SelectedSettings.Add(OutputSetting);
			SettingsWidget->SetSelectedSettings(SelectedSettings);
		}
		// Shot overrides may not have Output Settings.
		else
		{
			if (CachedPipelineConfig->GetUserSettings().Num() > 0)
			{
				SettingsWidget->SetSelectedSettings({ CachedPipelineConfig->GetUserSettings()[0] });
			}
		}
	}
	ChildSlot
	[
		SNew(SSplitter)
		.Orientation(Orient_Horizontal)
    
		+ SSplitter::Slot()
		.Value(.33f)
		[
			SNew(SBorder)
			.Padding(4)
			.BorderImage( FAppStyle::GetBrush("ToolPanel.GroupBorder") )
			[
				SettingsWidget.ToSharedRef()
			]
		]
    
		+ SSplitter::Slot()
		.Value(.67f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.FillHeight(1.f)
			[
				DetailsBox.ToSharedRef()
			]
			+ SVerticalBox::Slot()
			.Padding(FMargin(0, 4, 0, 0))
			.AutoHeight()
			[
				SNew(SBorder)
				.Visibility(this, &SMoviePipelineConfigEditor::IsSettingFooterVisible)
				.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
				[
					SNew(SBox)
					.MaxDesiredHeight(96)
					[
						SNew(SScrollBox)
						+ SScrollBox::Slot()
						[
							SNew(SMultiLineEditableText)
							.IsReadOnly(true)
							.AutoWrapText(true)
							.SelectWordOnMouseDoubleClick(true)
							.Text(this, &SMoviePipelineConfigEditor::GetSettingsFooterText)
						]
					]	
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
				.Visibility(this, &SMoviePipelineConfigEditor::IsValidationWarningVisible)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(2, 0, 4, 0)
					[
						SNew(STextBlock)
						.Font(FAppStyle::Get().GetFontStyle("FontAwesome.9"))
						.Text(FEditorFontGlyphs::Exclamation_Triangle)
						.ColorAndOpacity(FLinearColor::Yellow)
					]

					+ SHorizontalBox::Slot()
					.FillWidth(1.f)
					[
						SNew(STextBlock)
						.AutoWrapText(true)
						.Text(this, &SMoviePipelineConfigEditor::GetValidationWarningText)
					]
				]
			]
		]
	];

	// Register a callback so that we can refresh the details panel after it is reinstanced.
	GEditor->OnBlueprintReinstanced().AddSP(this, &SMoviePipelineConfigEditor::OnBlueprintReinstanced);
}

TSharedRef<SWidget> SMoviePipelineConfigEditor::MakeAddSettingButton()
{
	return SNew(SPositiveActionButton)
		.OnGetMenuContent(this, &SMoviePipelineConfigEditor::OnGenerateSettingsMenu)
		.Icon(FAppStyle::Get().GetBrush("Icons.Plus"))
		.Text(LOCTEXT("AddNewSetting_Text", "Setting"));
}
UE_ENABLE_OPTIMIZATION_SHIP

void SMoviePipelineConfigEditor::OnBlueprintReinstanced()
{
	SettingsWidget->InvalidateCachedSettingsSerialNumber();
	bRequestDetailsRefresh = true;
}

void SMoviePipelineConfigEditor::CheckForNewSettingsObject()
{
	UMoviePipelineConfigBase* NewMoviePipeline = PipelineConfigAttribute.Get();
	if (CachedPipelineConfig != NewMoviePipeline)
	{
		CachedPipelineConfig = NewMoviePipeline;

		SettingsWidget->SetShotConfigObject(NewMoviePipeline);
		bRequestDetailsRefresh = true;
	}

	UMoviePipelineExecutorJob* NewOwningJob = OwningJobAttribute.Get();
	if (CachedOwningJob != NewOwningJob)
	{
		CachedOwningJob = NewOwningJob;
		bRequestDetailsRefresh = true;
	}
}

void SMoviePipelineConfigEditor::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	CheckForNewSettingsObject();
	if (bRequestDetailsRefresh)
	{
		UpdateDetails();
		bRequestDetailsRefresh = false;
	}
}

TSharedRef<SWidget> SMoviePipelineConfigEditor::OnGenerateSettingsMenu()
{
	TSharedRef<FExtender> Extender = MakeShared<FExtender>();
	FMenuBuilder MenuBuilder(true, nullptr, Extender);

	// Put the different categories into different sections
	TArray<UClass*> SourceClasses = UE::MovieRenderPipeline::FindMoviePipelineSettingClasses(UMoviePipelineSetting::StaticClass());

	TMap<FString, TArray<UClass*>> CategorizedClasses;

	for (UClass* Class : SourceClasses)
	{
		const UMoviePipelineSetting* SettingDefaultObject = GetDefault<UMoviePipelineSetting>(Class);
		CategorizedClasses.FindOrAdd(SettingDefaultObject->GetCategoryText().ToString()).Add(Class);
	}

	for (TPair <FString, TArray<UClass*>> KVP : CategorizedClasses)
	{
		MenuBuilder.BeginSection(NAME_None, FText::FromString(KVP.Key));

		// Sort the classes by their CDO's GetDisplayText and not the classes GetDisplayName.
		Algo::Sort(KVP.Value, [](UClass* A, UClass* B)
			{
				const UMoviePipelineSetting* DefaultA = GetDefault<UMoviePipelineSetting>(A);
				const UMoviePipelineSetting* DefaultB = GetDefault<UMoviePipelineSetting>(B);

				return DefaultA->GetDisplayText().CompareTo(DefaultB->GetDisplayText()) < 0;
			});

		for (UClass* Class : KVP.Value)
		{
			// Get a display name for the setting from the CDO.
			const UMoviePipelineSetting* SettingDefaultObject = GetDefault<UMoviePipelineSetting>(Class);
			if (!SettingDefaultObject)
			{
				continue;
			}

			// Depending on the type of config we're editing, some settings may not be eligible. If this is the case, we omit them from the list.
			bool bCanSettingBeAdded = CachedPipelineConfig->CanSettingBeAdded(SettingDefaultObject);
			if (!bCanSettingBeAdded)
			{
				continue;
			}

			// If the setting already exists and it only allows a single instance, we omit them from the list.
			bool bAllowDuplicates = true;
			for (UMoviePipelineSetting* ExistingSetting : CachedPipelineConfig->GetUserSettings())
			{
				// If we found a setting with the same class as ours, ask the CDO if multiple are valid.
				if (ExistingSetting->GetClass() == Class)
				{
					bAllowDuplicates = !SettingDefaultObject->IsSolo();
					break;
				}
			}
			if (!bAllowDuplicates)
			{
				continue;
			}

			FText SettingDisplayName = SettingDefaultObject->GetDisplayText();

			TSubclassOf<UMoviePipelineSetting> SubclassOf = Class;
			MenuBuilder.AddMenuEntry(
				SettingDisplayName,
				Class->GetToolTipText(true),
				FSlateIconFinder::FindIconForClass(Class),
				FUIAction(
					FExecuteAction::CreateSP(this, &SMoviePipelineConfigEditor::AddSettingFromClass, SubclassOf),
					FCanExecuteAction::CreateSP(this, &SMoviePipelineConfigEditor::CanAddSettingFromClass, SubclassOf)
				)
			);
		}

		MenuBuilder.EndSection();
	}

	return MenuBuilder.MakeWidget();
}

void SMoviePipelineConfigEditor::UpdateDetails()
{
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::Get().LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bAllowSearch = false;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsViewArgs.bHideSelectionTip = true;
	DetailsViewArgs.bShowScrollBar = false;
	DetailsViewArgs.ColumnWidth = 0.5f;
	DetailsViewArgs.DefaultsOnlyVisibility = EEditDefaultsOnlyNodeVisibility::Hide;

	TArray<UMoviePipelineSetting*> SelectedSources;
	SettingsWidget->GetSelectedSettings(SelectedSources);

	
	// Create 1 details panel per source class type
	TSortedMap<const UClass*, TArray<UObject*>> ClassToSources;
	for (UMoviePipelineSetting* Source : SelectedSources)
	{
		ClassToSources.FindOrAdd(Source->GetClass()).Add(Source);
	}
	
	TArray<FObjectKey> PreviousClasses;
	ClassToDetailsView.GenerateKeyArray(PreviousClasses);

	TSharedPtr<INumericTypeInterface<double>> NumericTypeInterface = MakeShareable(new TDefaultNumericTypeInterface<double>);

	for (auto& Pair : ClassToSources)
	{
		PreviousClasses.Remove(Pair.Key);
	
		TSharedPtr<IDetailsView> ExistingDetails = ClassToDetailsView.FindRef(Pair.Key);
		if (ExistingDetails.IsValid())
		{
			ExistingDetails->SetObjects(Pair.Value);
		}
		else
		{
			TSharedRef<IDetailsView> Details = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
			Details->RegisterInstancedCustomPropertyTypeLayout("FrameNumber", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FFrameNumberDetailsCustomization::MakeInstance, NumericTypeInterface));
			Details->RegisterInstancedCustomPropertyLayout(UMoviePipelineOutputSetting::StaticClass(), FOnGetDetailCustomizationInstance::CreateStatic(FOutputFormatDetailsCustomization::MakeInstance, SharedThis(this)));
			Details->RegisterInstancedCustomPropertyLayout(UMoviePipelineConsoleVariableSetting::StaticClass(), FOnGetDetailCustomizationInstance::CreateStatic(FConsoleVariablesSettingDetailsCustomization::MakeInstance));

			Details->SetObjects(Pair.Value);
			DetailsBox->AddSlot()
				[
					Details
				];
	
			ClassToDetailsView.Add(Pair.Key, Details);
		}
	}
	
	for (FObjectKey StaleClass : PreviousClasses)
	{
		TSharedPtr<IDetailsView> Details = ClassToDetailsView.FindRef(StaleClass);
		DetailsBox->RemoveSlot(Details.ToSharedRef());
		ClassToDetailsView.Remove(StaleClass);
	}
}

void SMoviePipelineConfigEditor::AddSettingFromClass(TSubclassOf<UMoviePipelineSetting> SettingClass)
{
	UMoviePipelineConfigBase* Pipeline = PipelineConfigAttribute.Get();

	if (*SettingClass && Pipeline)
	{
		FScopedTransaction Transaction(FText::Format(LOCTEXT("AddNewSetting", "Add New {0} Setting"), SettingClass->GetDisplayNameText()));

		const bool bIncludeDisabledSettings = false;
		const bool bExactMatch = true;
		UMoviePipelineSetting* NewSetting = Pipeline->FindOrAddSettingByClass(SettingClass, bIncludeDisabledSettings, bExactMatch);

		SettingsWidget->SetSelectedSettings({ NewSetting });
	}
}

bool SMoviePipelineConfigEditor::CanAddSettingFromClass(TSubclassOf<UMoviePipelineSetting> SettingClass)
{
	return true;
}

EVisibility SMoviePipelineConfigEditor::IsSettingFooterVisible() const
{
	return (GetSettingsFooterText().IsEmpty()) ? EVisibility::Collapsed : EVisibility::Visible;
}

FText SMoviePipelineConfigEditor::GetSettingsFooterText() const
{
	TArray<UMoviePipelineSetting*> SelectedSettings;
	SettingsWidget->GetSelectedSettings(SelectedSettings);

	FTextBuilder TextBuilder;

	if (CachedOwningJob.IsValid())
	{
		for (const UMoviePipelineSetting* Setting : SelectedSettings)
		{
			const FText FooterText = Setting->GetFooterText(CachedOwningJob.Get());
			if (!FooterText.IsEmpty())
			{
				TextBuilder.AppendLine(FooterText);
			}
		}
	}

	return TextBuilder.ToText();
}

EVisibility SMoviePipelineConfigEditor::IsValidationWarningVisible() const
{
	TArray<UMoviePipelineSetting*> SelectedSettings;
	SettingsWidget->GetSelectedSettings(SelectedSettings);
	
	EMoviePipelineValidationState ValidationResult = EMoviePipelineValidationState::Valid;
	for (const UMoviePipelineSetting* Setting : SelectedSettings)
	{
		// Don't show warnings for disabled settings as the invalid setting won't be used when rendering.
		if (!Setting->IsEnabled())
		{
			continue;
		}

		if ((int32)Setting->GetValidationState() > (int32)ValidationResult)
		{
			ValidationResult = Setting->GetValidationState();
		}
	}

	return (ValidationResult != EMoviePipelineValidationState::Valid) ? EVisibility::Visible : EVisibility::Collapsed;
}

FText SMoviePipelineConfigEditor::GetValidationWarningText() const
{
	TArray<UMoviePipelineSetting*> SelectedSettings;
	SettingsWidget->GetSelectedSettings(SelectedSettings);

	FTextBuilder TextBuilder;
	for (const UMoviePipelineSetting* Setting : SelectedSettings)
	{
		for (const FText& Result : Setting->GetValidationResults())
		{
			TextBuilder.AppendLine(Result);
		}
	}

	return TextBuilder.ToText();
}
#undef LOCTEXT_NAMESPACE