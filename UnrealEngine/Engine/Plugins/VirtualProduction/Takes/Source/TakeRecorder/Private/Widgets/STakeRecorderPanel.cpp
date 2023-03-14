// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/STakeRecorderPanel.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "Widgets/TakeRecorderWidgetConstants.h"
#include "Widgets/SLevelSequenceTakeEditor.h"
#include "Recorder/TakeRecorder.h"
#include "Recorder/TakeRecorderPanel.h"
#include "ScopedSequencerPanel.h"
#include "ITakeRecorderModule.h"
#include "TakesCoreBlueprintLibrary.h"
#include "TakePreset.h"
#include "TakeMetaData.h"
#include "TakeRecorderSources.h"
#include "TakeRecorderStyle.h"
#include "TakeRecorderSettings.h"
#include "Widgets/STakeRecorderCockpit.h"
#include "LevelSequence.h"

// Core includes
#include "Modules/ModuleManager.h"
#include "Algo/Sort.h"
#include "Misc/FileHelper.h"

// AssetRegistry includes
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetRegistry/AssetData.h"

// ContentBrowser includes
#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"

// AssetTools includes
#include "AssetToolsModule.h"

// Slate includes
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Images/SImage.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/SlateIconFinder.h"

// Style includes
#include "Styling/AppStyle.h"
#include "EditorFontGlyphs.h"

// UnrealEd includes
#include "ScopedTransaction.h"
#include "FileHelpers.h"
#include "Misc/MessageDialog.h"

// Sequencer includes
#include "ISequencer.h"
#include "SequencerSettings.h"

#include "LevelEditor.h"

#define LOCTEXT_NAMESPACE "STakeRecorderPanel"

STakeRecorderPanel::~STakeRecorderPanel()
{
	UTakeRecorder::OnRecordingInitialized().Remove(OnRecordingInitializedHandle);
	ITakeRecorderModule& TakeRecorderModule = FModuleManager::Get().LoadModuleChecked<ITakeRecorderModule>("TakeRecorder");
	TakeRecorderModule.OnForceSaveAsPreset().Unbind();
	TakeRecorderModule.GetLastLevelSequenceProvider().Unbind();
	TakeRecorderModule.GetExternalObjectAddRemoveEventDelegate().Remove(OnWidgetExternalObjectChangedHandle);
}

PRAGMA_DISABLE_OPTIMIZATION
void STakeRecorderPanel::Construct(const FArguments& InArgs)
{
	// If a recording is currently underway, initialize to that now
	if (UTakeRecorder* ActiveRecorder = UTakeRecorder::GetActiveRecorder())
	{
		RecordingLevelSequence = ActiveRecorder->GetSequence();
		OnRecordingFinishedHandle = ActiveRecorder->OnRecordingFinished().AddSP(this, &STakeRecorderPanel::OnRecordingFinished);
		OnRecordingCancelledHandle = ActiveRecorder->OnRecordingCancelled().AddSP(this, &STakeRecorderPanel::OnRecordingCancelled);
	}
	else
	{
		RecordingLevelSequence = nullptr;
	}

	TransientPreset = AllocateTransientPreset();
	LastRecordedLevelSequence = nullptr;

	// Copy the base preset into the transient preset if it was provided.
	// We do this first so that anything that asks for its Level Sequence
	// on construction gets the right one
	if (InArgs._BasePreset)
	{
		TransientPreset->CopyFrom(InArgs._BasePreset);
	}
	else if (InArgs._BaseSequence)
	{
		TransientPreset->CopyFrom(InArgs._BaseSequence);

		ULevelSequence* LevelSequence = TransientPreset->GetLevelSequence();

		UTakeRecorderSources* BaseSources = InArgs._BaseSequence->FindMetaData<UTakeRecorderSources>();
		if (BaseSources && LevelSequence)
		{
			LevelSequence->CopyMetaData(BaseSources);
		}

		if (LevelSequence)
		{
			LevelSequence->GetMovieScene()->SetReadOnly(false);
		}

		UTakeMetaData*  TakeMetaData = LevelSequence ? LevelSequence->FindMetaData<UTakeMetaData>() : nullptr;
		if (TakeMetaData)
		{
			TakeMetaData->Unlock();
			TakeMetaData->SetTimestamp(FDateTime(0));
		}
	}
	else if (InArgs._RecordIntoSequence)
	{
		SetRecordIntoLevelSequence(InArgs._RecordIntoSequence);
	}
	else if (InArgs._SequenceToView)
	{
		SuppliedLevelSequence  = InArgs._SequenceToView;
	}

	/** Clear the dirty flag since the preset was just initialized. */
	TransientPreset->GetOutermost()->SetDirtyFlag(false);

	// Create the child widgets that need to know about our level sequence
	CockpitWidget = SNew(STakeRecorderCockpit)
	.LevelSequence(this, &STakeRecorderPanel::GetLevelSequence)
	.TakeRecorderMode(this, &STakeRecorderPanel::GetTakeRecorderMode);

	LevelSequenceTakeWidget = SNew(SLevelSequenceTakeEditor)
	.LevelSequence(this, &STakeRecorderPanel::GetLevelSequence);

	// Create the sequencer panel, and open it if necessary
	SequencerPanel = MakeShared<FScopedSequencerPanel>(MakeAttributeSP(this, &STakeRecorderPanel::GetLevelSequence));

	// Bind onto the necessary delegates we need
	OnLevelSequenceChangedHandle = TransientPreset->AddOnLevelSequenceChanged(FSimpleDelegate::CreateSP(this, &STakeRecorderPanel::OnLevelSequenceChanged));
	OnRecordingInitializedHandle = UTakeRecorder::OnRecordingInitialized().AddSP(this, &STakeRecorderPanel::OnRecordingInitialized);

	ITakeRecorderModule& TakeRecorderModule = FModuleManager::Get().LoadModuleChecked<ITakeRecorderModule>("TakeRecorder");
	OnWidgetExternalObjectChangedHandle =
		TakeRecorderModule.GetExternalObjectAddRemoveEventDelegate().AddSP(this, &STakeRecorderPanel::ReconfigureExternalSettings);

	TakeRecorderModule.OnForceSaveAsPreset().BindRaw(this, &STakeRecorderPanel::OnSaveAsPreset);
	TakeRecorderModule.GetLastLevelSequenceProvider().BindRaw(this, &STakeRecorderPanel::SetLastLevelSequence);

	for(TWeakObjectPtr<> Object : TakeRecorderModule.GetExternalObjects())
	{
		if (Object.IsValid())
		{
			LevelSequenceTakeWidget->AddExternalSettingsObject(Object.Get());
		}
	}

	// Setup the preset origin for the meta-data in the cockpit if one was supplied
	if (InArgs._BasePreset)
	{
		CockpitWidget->GetMetaData()->SetPresetOrigin(InArgs._BasePreset);
	}

	// Add the settings immediately if the user preference tells us to
	UTakeRecorderUserSettings* UserSettings = GetMutableDefault<UTakeRecorderUserSettings>();
	UTakeRecorderProjectSettings* ProjectSettings = GetMutableDefault<UTakeRecorderProjectSettings>();
	if (UserSettings->bShowUserSettingsOnUI)
	{
		LevelSequenceTakeWidget->AddExternalSettingsObject(ProjectSettings);
		LevelSequenceTakeWidget->AddExternalSettingsObject(UserSettings);
	}

	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.Padding(FMargin(0.f, 1.0f))
		.AutoHeight()
		[
			MakeToolBar()
		]

		+ SVerticalBox::Slot()
		.Padding(FMargin(0.f, 1.0f))
		.AutoHeight()
		[
			CockpitWidget.ToSharedRef()
		]

		+ SVerticalBox::Slot()
		.Padding(FMargin(0.f, 1.0f, 0.f, 0.f))
		.AutoHeight()
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("DetailsView.CategoryTop"))
			.BorderBackgroundColor( FLinearColor( .6,.6,.6, 1.0f ) )
			.IsEnabled_Lambda([this]() { return !CockpitWidget->Reviewing() && !CockpitWidget->Recording(); })
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.Padding(TakeRecorder::ButtonOffset)
				.VAlign(VAlign_Fill)
				.AutoWidth()
				[
					LevelSequenceTakeWidget->MakeAddSourceButton()
				]

				+ SHorizontalBox::Slot()
				.Padding(TakeRecorder::ButtonOffset)
				.VAlign(VAlign_Fill)
				.AutoWidth()
				[
					SNew(SComboButton)
					.ContentPadding(TakeRecorder::ButtonPadding)
					.ComboButtonStyle(FTakeRecorderStyle::Get(), "ComboButton")
					.OnGetMenuContent(this, &STakeRecorderPanel::OnGeneratePresetsMenu)
					.ForegroundColor(FSlateColor::UseForeground())
					.ButtonContent()
					[
						SNew(SHorizontalBox)

						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(SImage)
							.Image(FSlateIconFinder::FindIconBrushForClass(UTakePreset::StaticClass()))
						]

						+ SHorizontalBox::Slot()
						[
							SNew(STextBlock)
							.Text(LOCTEXT("PresetsToolbarButton", "Presets"))
						]
					]
				]

				+ SHorizontalBox::Slot()
				[
					SNew(SSpacer)
				]

				+ SHorizontalBox::Slot()
				.Padding(TakeRecorder::ButtonOffset)
				.VAlign(VAlign_Fill)
				.AutoWidth()
				[
					SNew(SButton)
					.ContentPadding(TakeRecorder::ButtonPadding)
					.ToolTipText(LOCTEXT("RevertChanges_Text", "Revert all changes made to this take back its original state (either its original preset, or an empty take)."))
					.ForegroundColor(FSlateColor::UseForeground())
					.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
					.OnClicked(this, &STakeRecorderPanel::OnRevertChanges)
					[
						SNew(STextBlock)
						.Font(FAppStyle::Get().GetFontStyle("FontAwesome.11"))
						.Text(FEditorFontGlyphs::Undo)
					]
				]
			]
		]

		+ SVerticalBox::Slot()
		[
			SNew(SHorizontalBox)
			.IsEnabled_Lambda([this]() { return !CockpitWidget->Recording(); })
			+ SHorizontalBox::Slot()
			[
				LevelSequenceTakeWidget.ToSharedRef()
			]
		]
	];
}

TSharedRef<SWidget> STakeRecorderPanel::MakeToolBar()
{
	int32 ButtonBoxSize = 28;
	TSharedPtr<SHorizontalBox> ButtonHolder;

	TSharedRef<SBorder> Border = SNew(SBorder)
	.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
	.Padding(FMargin(3.f, 3.f))
	[

		SAssignNew(ButtonHolder, SHorizontalBox)

		+ SHorizontalBox::Slot()
		.Padding(TakeRecorder::ButtonOffset)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(SBox)
			.WidthOverride(ButtonBoxSize)
			.HeightOverride(ButtonBoxSize)
			[
				SNew(SButton)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.ToolTipText(LOCTEXT("ClearPendingTake", "Clear pending take"))
				.ForegroundColor(FSlateColor::UseForeground())
				.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
				.OnClicked(this, &STakeRecorderPanel::OnClearPendingTake)
				[
					SNew(STextBlock)
					.Font(FAppStyle::Get().GetFontStyle("FontAwesome.14"))
					.Text(FEditorFontGlyphs::File)
				]
			]
		]

		+ SHorizontalBox::Slot()
		.Padding(TakeRecorder::ButtonOffset)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[

			SNew(SBox)
			.WidthOverride(ButtonBoxSize)
			.HeightOverride(ButtonBoxSize)
			.Visibility_Lambda([this]() { return !CockpitWidget->Reviewing() ? EVisibility::Visible : EVisibility::Collapsed; })
			[
				SNew(SButton)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.ContentPadding(TakeRecorder::ButtonPadding)
				.ToolTipText(LOCTEXT("ReviewLastRecording", "Review the last recording"))
				.ForegroundColor(FSlateColor::UseForeground())
				.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
				.IsEnabled_Lambda([this]() { return (CanReviewLastLevelSequence() && GetTakeRecorderMode() == ETakeRecorderMode::RecordNewSequence); })
				.OnClicked(this, &STakeRecorderPanel::OnReviewLastRecording)
				[
					SNew(SImage)
					.Image(FTakeRecorderStyle::Get().GetBrush("TakeRecorder.ReviewRecordingButton"))
				]
			]
		]

		+ SHorizontalBox::Slot()
		.Padding(TakeRecorder::ButtonOffset)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(SBox)
			.WidthOverride(ButtonBoxSize)
			.HeightOverride(ButtonBoxSize)
			.Visibility_Lambda([this]() { return CockpitWidget->Reviewing() ? EVisibility::Visible : EVisibility::Collapsed; })
			[
				SNew(SButton)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.ContentPadding(TakeRecorder::ButtonPadding)
				.ToolTipText(LOCTEXT("Back", "Return back to the pending take"))
				.ForegroundColor(FSlateColor::UseForeground())
				.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
				.OnClicked(this, &STakeRecorderPanel::OnBackToPendingTake)
				[
					SNew(STextBlock)
					.Font(FAppStyle::Get().GetFontStyle("FontAwesome.14"))
					.Text(FEditorFontGlyphs::Arrow_Left)
				]
			]
		]

		+ SHorizontalBox::Slot()
		.Padding(TakeRecorder::ButtonOffset)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(SBox)
			.WidthOverride(ButtonBoxSize)
			.HeightOverride(ButtonBoxSize)
			.Visibility_Lambda([this]() { return !CockpitWidget->Reviewing() ? EVisibility::Visible : EVisibility::Collapsed; })
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SBox)
					.WidthOverride(ButtonBoxSize)
					.HeightOverride(ButtonBoxSize)
					[
						SNew(SCheckBox)
						.ToolTipText_Lambda([this]() { return GetTakeRecorderMode() == ETakeRecorderMode::RecordIntoSequence ? LOCTEXT("RecordIntoSequenceTooltip", "Recording directly into chosen sequence") : LOCTEXT("RecordFromPendingTakeTooltip", "Recording from pending take. To record into an existing sequence, choose a sequence to record into"); })
						.Style(FTakeRecorderStyle::Get(), "ToggleButtonIndicatorCheckbox")
						.IsChecked_Lambda([this]() { return GetTakeRecorderMode() == ETakeRecorderMode::RecordIntoSequence ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
						[
							SNew(SImage)
							.Image(FTakeRecorderStyle::Get().GetBrush("TakeRecorder.SequenceToRecordIntoButton"))
						]
					]
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2)
				[
					SNew(SComboButton)
					.ContentPadding(2)
					.ForegroundColor(FSlateColor::UseForeground())
					.ComboButtonStyle(FTakeRecorderStyle::Get(), "ComboButton")
					.ToolTipText(LOCTEXT("OpenSequenceToRecordIntoTooltip", "Open sequence to record into"))
					.OnGetMenuContent(this, &STakeRecorderPanel::OnOpenSequenceToRecordIntoMenu)
					.HasDownArrow(false)
					.ButtonContent()
					[
						SNew(STextBlock)
						.TextStyle(FAppStyle::Get(), "NormalText.Important")
						.Font(FAppStyle::Get().GetFontStyle("FontAwesome.10"))
						.Text(FEditorFontGlyphs::Caret_Down)
					]
				]
			]
		]

		+ SHorizontalBox::Slot()
		[
			SNew(SSpacer)
		]

		+ SHorizontalBox::Slot()
		.Padding(TakeRecorder::ButtonOffset)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(SBox)
			.WidthOverride(ButtonBoxSize)
			.HeightOverride(ButtonBoxSize)
			[
				SNew(SCheckBox)
				.Padding(TakeRecorder::ButtonPadding)
				.ToolTipText(NSLOCTEXT("TakesBrowser", "ToggleTakeBrowser_Tip", "Show/Hide the Takes Browser"))
				.Style(FTakeRecorderStyle::Get(), "ToggleButtonCheckbox")
				.IsChecked(this, &STakeRecorderPanel::GetTakeBrowserCheckState)
				.OnCheckStateChanged(this, &STakeRecorderPanel::ToggleTakeBrowserCheckState)
				[
					SNew(STextBlock)
					.Font(FAppStyle::Get().GetFontStyle("FontAwesome.14"))
					.Text(FEditorFontGlyphs::Folder_Open)
				]
			]
		]

		+ SHorizontalBox::Slot()
		.Padding(TakeRecorder::ButtonOffset)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(SBox)
			.WidthOverride(ButtonBoxSize)
			.HeightOverride(ButtonBoxSize)
			[

				SequencerPanel->MakeToggleButton()
			]
		]

		+ SHorizontalBox::Slot()
		.Padding(TakeRecorder::ButtonOffset)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(SBox)
			.WidthOverride(ButtonBoxSize)
			.HeightOverride(ButtonBoxSize)
			.Visibility_Lambda([this]() { return CockpitWidget->Reviewing() ? EVisibility::Visible : EVisibility::Collapsed; })
			[
				CockpitWidget->MakeLockButton()
			]
		]

		+ SHorizontalBox::Slot()
		.Padding(TakeRecorder::ButtonOffset)
		.VAlign(VAlign_Fill)
		.AutoWidth()
		[
			SNew(SBox)
			.WidthOverride(ButtonBoxSize)
			.HeightOverride(ButtonBoxSize)
			[
				SNew(SCheckBox)
				.Padding(TakeRecorder::ButtonPadding)
				.ToolTipText(LOCTEXT("ShowSettings_Tip", "Show/Hide the general user/project settings for Take Recorder"))
				.Style(FAppStyle::Get(), "ToggleButtonCheckbox")
				.ForegroundColor(FSlateColor::UseForeground())
				.IsChecked(this, &STakeRecorderPanel::GetSettingsCheckState)
				.OnCheckStateChanged(this, &STakeRecorderPanel::ToggleSettings)
				.Visibility_Lambda([this]() { return !CockpitWidget->Reviewing() ? EVisibility::Visible : EVisibility::Collapsed; })
				[
					SNew(STextBlock)
					.Font(FAppStyle::Get().GetFontStyle("FontAwesome.14"))
					.Text(FEditorFontGlyphs::Cogs)
				]
			]
		]
	];

	ITakeRecorderModule& TakeRecorderModule = FModuleManager::Get().LoadModuleChecked<ITakeRecorderModule>("TakeRecorder");
	TArray<TSharedRef<SWidget>> OutExtensions;
	TakeRecorderModule.GetToolbarExtensionGenerators().Broadcast(OutExtensions);

	for (const TSharedRef<SWidget>& Widget : OutExtensions)
	{
		ButtonHolder->AddSlot()
		.Padding(TakeRecorder::ButtonOffset)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(SBox)
			.HeightOverride(ButtonBoxSize)
			[
				Widget
			]
		];
	}

	return Border;
}
PRAGMA_ENABLE_OPTIMIZATION

ULevelSequence* STakeRecorderPanel::GetLevelSequence() const
{
	if (SuppliedLevelSequence)
	{
		return SuppliedLevelSequence;
	}
	else if (RecordIntoLevelSequence)
	{
		return RecordIntoLevelSequence;
	}
	else if (RecordingLevelSequence)
	{
		return RecordingLevelSequence;
	}
	else
	{
		return TransientPreset->GetLevelSequence();
	}
}

ULevelSequence* STakeRecorderPanel::GetLastRecordedLevelSequence() const
{
	return LastRecordedLevelSequence;
}

void STakeRecorderPanel::SetLastLevelSequence(ULevelSequence *InSequence)
{
	LastRecordedLevelSequence = InSequence;
}

bool STakeRecorderPanel::CanReviewLastLevelSequence() const
{
	if (LastRecordedLevelSequence == nullptr)
	{
		return false;
	}
	ITakeRecorderModule& TakeRecorderModule = FModuleManager::Get().LoadModuleChecked<ITakeRecorderModule>("TakeRecorder");
	FCanReviewLastRecordedLevelSequence& CanReview = TakeRecorderModule.GetCanReviewLastRecordedLevelSequenceDelegate();
	if (CanReview.IsBound())
	{
		return CanReview.Execute();
	}
	
	return true;
}

ETakeRecorderMode STakeRecorderPanel::GetTakeRecorderMode() const
{
	if (RecordIntoLevelSequence != nullptr)
	{
		return ETakeRecorderMode::RecordIntoSequence;
	}

	return ETakeRecorderMode::RecordNewSequence;
}

UTakeMetaData* STakeRecorderPanel::GetTakeMetaData() const
{
	return CockpitWidget->GetMetaData();
}

void STakeRecorderPanel::ClearPendingTake()
{
	if (CockpitWidget->Reviewing())
	{
		LastRecordedLevelSequence = SuppliedLevelSequence;
	}

	UTakeRecorderSources* BaseSources = nullptr;

	if (ULevelSequence* CurrentLevelSequence = GetLevelSequence())
	{
		BaseSources = CurrentLevelSequence->FindMetaData<UTakeRecorderSources>();
	}

	SuppliedLevelSequence = nullptr;
	RecordIntoLevelSequence = nullptr;

	FScopedTransaction Transaction(LOCTEXT("ClearPendingTake_Transaction", "Clear Pending Take"));

	TransientPreset->Modify();
	TransientPreset->CreateLevelSequence();

	ULevelSequence* LevelSequence = TransientPreset->GetLevelSequence();
	if (LevelSequence && BaseSources)
	{
		LevelSequence->CopyMetaData(BaseSources);
	}
}

TOptional<ETakeRecorderPanelMode> STakeRecorderPanel::GetMode() const
{
	if (SuppliedLevelSequence)
	{
		return ETakeRecorderPanelMode::ReviewingRecording;	
	}
	else if (RecordingLevelSequence)
	{
		return ETakeRecorderPanelMode::NewRecording;	
	}
	else if (RecordIntoLevelSequence)
	{
		return ETakeRecorderPanelMode::RecordingInto;	
	}

	return TOptional<ETakeRecorderPanelMode>();
}

UTakePreset* STakeRecorderPanel::AllocateTransientPreset()
{
	return UTakePreset::AllocateTransientPreset(GetDefault<UTakeRecorderUserSettings>()->LastOpenedPreset.Get());
}


void STakeRecorderPanel::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(TransientPreset);
	Collector.AddReferencedObject(RecordIntoLevelSequence);
	Collector.AddReferencedObject(SuppliedLevelSequence);
	Collector.AddReferencedObject(RecordingLevelSequence);
	Collector.AddReferencedObject(LastRecordedLevelSequence);
}


TSharedRef<SWidget> STakeRecorderPanel::OnGeneratePresetsMenu()
{
	FMenuBuilder MenuBuilder(true, nullptr);

	IContentBrowserSingleton& ContentBrowser = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser").Get();

	MenuBuilder.AddMenuEntry(
		LOCTEXT("SaveAsPreset_Text", "Save As Preset"),
		LOCTEXT("SaveAsPreset_Tip", "Save the current setup as a new preset that can be imported at a later date"),
		FSlateIcon(FAppStyle::Get().GetStyleSetName(), "AssetEditor.SaveAsset"),
		FUIAction(
			FExecuteAction::CreateSP(this, &STakeRecorderPanel::OnSaveAsPreset)
		)
	);

	FAssetPickerConfig AssetPickerConfig;
	{
		AssetPickerConfig.SelectionMode = ESelectionMode::Single;
		AssetPickerConfig.InitialAssetViewType = EAssetViewType::Column;
		AssetPickerConfig.bFocusSearchBoxWhenOpened = true;
		AssetPickerConfig.bAllowNullSelection = false;
		AssetPickerConfig.bShowBottomToolbar = true;
		AssetPickerConfig.bAutohideSearchBar = false;
		AssetPickerConfig.bAllowDragging = false;
		AssetPickerConfig.bCanShowClasses = false;
		AssetPickerConfig.bShowPathInColumnView = true;
		AssetPickerConfig.bShowTypeInColumnView = false;
		AssetPickerConfig.bSortByPathInColumnView = false;

		AssetPickerConfig.AssetShowWarningText = LOCTEXT("NoPresets_Warning", "No Presets Found");
		AssetPickerConfig.Filter.ClassPaths.Add(UTakePreset::StaticClass()->GetClassPathName());
		AssetPickerConfig.OnAssetSelected = FOnAssetSelected::CreateSP(this, &STakeRecorderPanel::OnImportPreset);
	}

	MenuBuilder.BeginSection(NAME_None, LOCTEXT("ImportPreset_MenuSection", "Import Preset"));
	{
		TSharedRef<SWidget> PresetPicker = SNew(SBox)
			.MinDesiredWidth(400.f)
			.MinDesiredHeight(400.f)
			[
				ContentBrowser.CreateAssetPicker(AssetPickerConfig)
			];

		MenuBuilder.AddWidget(PresetPicker, FText(), true, false);
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}


void STakeRecorderPanel::OnImportPreset(const FAssetData& InPreset)
{
	FSlateApplication::Get().DismissAllMenus();

	SuppliedLevelSequence = nullptr;
	RecordIntoLevelSequence = nullptr;

	UTakePreset* Take = CastChecked<UTakePreset>(InPreset.GetAsset());
	if (Take)
	{
		FScopedTransaction Transaction(LOCTEXT("ImportPreset_Transaction", "Import Take Preset"));

		TransientPreset->Modify();
		TransientPreset->CopyFrom(Take);
		TransientPreset->GetOutermost()->SetDirtyFlag(false);

		CockpitWidget->GetMetaData()->SetPresetOrigin(Take);
	}
	else
	{
		// @todo: notification?
	}
}


static bool OpenSaveDialog(const FString& InDefaultPath, const FString& InNewNameSuggestion, FString& OutPackageName)
{
	FSaveAssetDialogConfig SaveAssetDialogConfig;
	{
		SaveAssetDialogConfig.DefaultPath = InDefaultPath;
		SaveAssetDialogConfig.DefaultAssetName = InNewNameSuggestion;
		SaveAssetDialogConfig.AssetClassNames.Add(UTakePreset::StaticClass()->GetClassPathName());
		SaveAssetDialogConfig.ExistingAssetPolicy = ESaveAssetDialogExistingAssetPolicy::AllowButWarn;
		SaveAssetDialogConfig.DialogTitleOverride = LOCTEXT("SaveTakePresetDialogTitle", "Save Take Preset");
	}

	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	FString SaveObjectPath = ContentBrowserModule.Get().CreateModalSaveAssetDialog(SaveAssetDialogConfig);

	if (!SaveObjectPath.IsEmpty())
	{
		OutPackageName = FPackageName::ObjectPathToPackageName(SaveObjectPath);
		return true;
	}

	return false;
}


bool STakeRecorderPanel::GetSavePresetPackageName(FString& OutName)
{
	UTakeRecorderUserSettings* ConfigSettings = GetMutableDefault<UTakeRecorderUserSettings>();

	FDateTime Today = FDateTime::Now();

	TMap<FString, FStringFormatArg> FormatArgs;
	FormatArgs.Add(TEXT("date"), Today.ToString());

	// determine default package path
	const FString DefaultSaveDirectory = FString::Format(*ConfigSettings->PresetSaveDir.Path, FormatArgs);

	FString DialogStartPath;
	FPackageName::TryConvertFilenameToLongPackageName(DefaultSaveDirectory, DialogStartPath);
	if (DialogStartPath.IsEmpty())
	{
		DialogStartPath = TEXT("/Game");
	}

	// determine default asset name
	FString DefaultName = LOCTEXT("NewTakePreset", "NewTakePreset").ToString();

	FString UniquePackageName;
	FString UniqueAssetName;

	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
	AssetToolsModule.Get().CreateUniqueAssetName(DialogStartPath / DefaultName, TEXT(""), UniquePackageName, UniqueAssetName);

	FString DialogStartName = FPaths::GetCleanFilename(UniqueAssetName);

	FString UserPackageName;
	FString NewPackageName;

	// get destination for asset
	bool bFilenameValid = false;
	while (!bFilenameValid)
	{
		if (!OpenSaveDialog(DialogStartPath, DialogStartName, UserPackageName))
		{
			return false;
		}

		NewPackageName = FString::Format(*UserPackageName, FormatArgs);

		FText OutError;
		bFilenameValid = FFileHelper::IsFilenameValidForSaving(NewPackageName, OutError);
	}

	ConfigSettings->PresetSaveDir.Path = FPackageName::GetLongPackagePath(UserPackageName);
	ConfigSettings->SaveConfig();
	OutName = MoveTemp(NewPackageName);
	return true;
}

void STakeRecorderPanel::OnSaveAsPreset()
{
	FString PackageName;
	if (!GetSavePresetPackageName(PackageName))
	{
		return;
	}

	FScopedTransaction Transaction(LOCTEXT("SaveAsPreset", "Save As Preset"));

	// Saving into a new package
	const FString NewAssetName = FPackageName::GetLongPackageAssetName(PackageName);
	UPackage*     NewPackage   = CreatePackage(*PackageName);
	UTakePreset*  NewPreset    = NewObject<UTakePreset>(NewPackage, *NewAssetName, RF_Public | RF_Standalone | RF_Transactional);

	if (NewPreset)
	{
		NewPreset->CopyFrom(TransientPreset);
		if (ULevelSequence* LevelSequence = NewPreset->GetLevelSequence())
		{
			// Ensure no take meta data is saved with this preset
			LevelSequence->RemoveMetaData<UTakeMetaData>();
		}

		NewPreset->MarkPackageDirty();
		// Clear the package dirty flag on the transient preset since it was saved.
		TransientPreset->GetOutermost()->SetDirtyFlag(false);
		FAssetRegistryModule::AssetCreated(NewPreset);

		FEditorFileUtils::PromptForCheckoutAndSave({ NewPackage }, false, false);

		CockpitWidget->GetMetaData()->SetPresetOrigin(NewPreset);
	}
}

FReply STakeRecorderPanel::OnBackToPendingTake()
{
	if (CockpitWidget->Reviewing())
	{
		LastRecordedLevelSequence = SuppliedLevelSequence;
	}

	SuppliedLevelSequence = nullptr;
	RecordIntoLevelSequence = nullptr;
	
	TransientPreset = AllocateTransientPreset();
	RefreshPanel();

	return FReply::Handled();
}

FReply STakeRecorderPanel::OnClearPendingTake()
{
	FText WarningMessage (LOCTEXT("Warning_ClearPendingTake", "Are you sure you want to clear the pending take? Your current tracks will be discarded."));
	if (EAppReturnType::No == FMessageDialog::Open(EAppMsgType::YesNo, WarningMessage))
	{
		return FReply::Handled();
	}

	ClearPendingTake();
	return FReply::Handled();
}

FReply STakeRecorderPanel::OnReviewLastRecording()
{
	if (LastRecordedLevelSequence)
	{
		SuppliedLevelSequence = LastRecordedLevelSequence;
		LastRecordedLevelSequence = nullptr;
		RefreshPanel();
	}

	return FReply::Handled();
}

FReply STakeRecorderPanel::OnRevertChanges()
{
	FText WarningMessage(LOCTEXT("Warning_RevertChanges", "Are you sure you want to revert changes? Your current changes will be discarded."));
	if (EAppReturnType::No == FMessageDialog::Open(EAppMsgType::YesNo, WarningMessage))
	{
		return FReply::Handled();
	}

	UTakePreset* PresetOrigin = CockpitWidget->GetMetaData()->GetPresetOrigin();

	FScopedTransaction Transaction(LOCTEXT("RevertChanges_Transaction", "Revert Changes"));

	TransientPreset->Modify();
	TransientPreset->CopyFrom(PresetOrigin);
	TransientPreset->GetOutermost()->SetDirtyFlag(false);

	return FReply::Handled();
}

TSharedRef<SWidget> STakeRecorderPanel::OnOpenSequenceToRecordIntoMenu()
{
	FMenuBuilder MenuBuilder(true, nullptr);
	IContentBrowserSingleton& ContentBrowser = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser").Get();

	FAssetPickerConfig AssetPickerConfig;
	{
		AssetPickerConfig.SelectionMode = ESelectionMode::Single;
		AssetPickerConfig.InitialAssetViewType = EAssetViewType::List;
		AssetPickerConfig.bFocusSearchBoxWhenOpened = true;
		AssetPickerConfig.bAllowNullSelection = false;
		AssetPickerConfig.bShowBottomToolbar = true;
		AssetPickerConfig.bAutohideSearchBar = false;
		AssetPickerConfig.bAllowDragging = false;
		AssetPickerConfig.bCanShowClasses = false;
		AssetPickerConfig.bShowPathInColumnView = true;
		AssetPickerConfig.bShowTypeInColumnView = false;
		AssetPickerConfig.bSortByPathInColumnView = false;
		AssetPickerConfig.ThumbnailScale = 0.3f;
		AssetPickerConfig.SaveSettingsName = TEXT("TakeRecorderOpenSequenceToRecordInto");

		AssetPickerConfig.AssetShowWarningText = LOCTEXT("NoSequences_Warning", "No Level Sequences Found");
		AssetPickerConfig.Filter.ClassPaths.Add(ULevelSequence::StaticClass()->GetClassPathName());
		AssetPickerConfig.OnAssetSelected = FOnAssetSelected::CreateSP(this, &STakeRecorderPanel::OnOpenSequenceToRecordInto);
	}

	MenuBuilder.BeginSection(NAME_None, LOCTEXT("OpenSequenceToRecordInto", "Open Sequence to Record Into"));
	{
		TSharedRef<SWidget> PresetPicker = SNew(SBox)
			.WidthOverride(300.f)
			.HeightOverride(300.f)
			[
				ContentBrowser.CreateAssetPicker(AssetPickerConfig)
			];

		MenuBuilder.AddWidget(PresetPicker, FText(), true, false);
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

void STakeRecorderPanel::OnOpenSequenceToRecordInto(const FAssetData& InAsset)
{
	// Close the dropdown menu that showed them the assets to pick from.
	FSlateApplication::Get().DismissAllMenus();

	// Only try to initialize level sequences, in the event they had more than a level sequence selected when drag/dropping.
	ULevelSequence* LevelSequence = Cast<ULevelSequence>(InAsset.GetAsset());
	if (LevelSequence)
	{
		SetRecordIntoLevelSequence(LevelSequence);
		RefreshPanel();
	}
}

void STakeRecorderPanel::RefreshPanel()
{
	// Re-open the sequencer panel for the new level sequence if it should be
	if (GetDefault<UTakeRecorderUserSettings>()->bIsSequenceOpen)
	{
		SequencerPanel->Open();
	}
}

ECheckBoxState STakeRecorderPanel::GetSettingsCheckState() const
{
	return GetDefault<UTakeRecorderUserSettings>()->bShowUserSettingsOnUI ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}


void STakeRecorderPanel::ToggleSettings(ECheckBoxState CheckState)
{
	UTakeRecorderUserSettings* UserSettings = GetMutableDefault<UTakeRecorderUserSettings>();
	UTakeRecorderProjectSettings* ProjectSettings = GetMutableDefault<UTakeRecorderProjectSettings>();

	if (LevelSequenceTakeWidget->RemoveExternalSettingsObject(UserSettings))
	{
		LevelSequenceTakeWidget->RemoveExternalSettingsObject(ProjectSettings);
		UserSettings->bShowUserSettingsOnUI = false;
	}
	else
	{
		LevelSequenceTakeWidget->AddExternalSettingsObject(ProjectSettings);
		LevelSequenceTakeWidget->AddExternalSettingsObject(UserSettings);
		UserSettings->bShowUserSettingsOnUI = true;
	}

	UserSettings->SaveConfig();
}

void STakeRecorderPanel::OnLevelSequenceChanged()
{
	RefreshPanel();
}

void STakeRecorderPanel::SetRecordIntoLevelSequence(ULevelSequence* LevelSequence)
{
	SuppliedLevelSequence = nullptr;
	RecordIntoLevelSequence = LevelSequence;

	if (RecordIntoLevelSequence)
	{
		RecordIntoLevelSequence->GetMovieScene()->SetReadOnly(false);
	}

	UTakeMetaData*  TakeMetaData = RecordIntoLevelSequence ? RecordIntoLevelSequence->FindOrAddMetaData<UTakeMetaData>() : nullptr;
	if (TakeMetaData)
	{
		// Set up take metadata to match this level sequence's info, ie. match the frame rate, use the level sequence name as the slate
		TakeMetaData->Unlock();
		TakeMetaData->SetTimestamp(FDateTime(0));
		TakeMetaData->SetSlate(LevelSequence->GetName());
		TakeMetaData->SetTakeNumber(0);
		TakeMetaData->SetFrameRate(LevelSequence->GetMovieScene()->GetDisplayRate());
		TakeMetaData->SetFrameRateFromTimecode(false);
	}
}

void STakeRecorderPanel::OnRecordingInitialized(UTakeRecorder* Recorder)
{
	RecordingLevelSequence = Recorder->GetSequence();
	RefreshPanel();

	OnRecordingFinishedHandle = Recorder->OnRecordingFinished().AddSP(this, &STakeRecorderPanel::OnRecordingFinished);
	OnRecordingCancelledHandle = Recorder->OnRecordingCancelled().AddSP(this, &STakeRecorderPanel::OnRecordingCancelled);
}

void STakeRecorderPanel::OnRecordingFinished(UTakeRecorder* Recorder)
{
	LastRecordedLevelSequence = RecordingLevelSequence;
	OnRecordingCancelled(Recorder);

	// Update the preset take number at the end of recording
	ULevelSequence* LevelSequence = TransientPreset->GetLevelSequence();
	UTakeMetaData*  TakeMetaData = LevelSequence ? LevelSequence->FindMetaData<UTakeMetaData>() : nullptr;

	if (TakeMetaData)
	{
		int32 NextTakeNumber = UTakesCoreBlueprintLibrary::ComputeNextTakeNumber(TakeMetaData->GetSlate());
		TakeMetaData->SetTakeNumber(NextTakeNumber);
	}
}

void STakeRecorderPanel::OnRecordingCancelled(UTakeRecorder* Recorder)
{
	RecordingLevelSequence = nullptr;
	RefreshPanel();
	CockpitWidget->Refresh();

	Recorder->OnRecordingFinished().Remove(OnRecordingFinishedHandle);
	Recorder->OnRecordingCancelled().Remove(OnRecordingCancelledHandle);
}

ECheckBoxState STakeRecorderPanel::GetTakeBrowserCheckState() const
{
	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
	TSharedPtr<SDockTab> TakesBrowserTab = LevelEditorModule.GetLevelEditorTabManager()->FindExistingLiveTab(ITakeRecorderModule::TakesBrowserTabName);
	if (TakesBrowserTab.IsValid())
	{
		return TakesBrowserTab->IsForeground() ? ECheckBoxState::Checked : ECheckBoxState::Undetermined;
	}
	return ECheckBoxState::Unchecked;
}

void STakeRecorderPanel::ToggleTakeBrowserCheckState(ECheckBoxState CheckState)
{
	// If it is up, but not visible, then bring it forward
	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
	TSharedPtr<SDockTab> TakesBrowserTab = LevelEditorModule.GetLevelEditorTabManager()->FindExistingLiveTab(ITakeRecorderModule::TakesBrowserTabName);
	if (TakesBrowserTab.IsValid())
	{
		if (!TakesBrowserTab->IsForeground())
		{
			TakesBrowserTab->ActivateInParent(ETabActivationCause::SetDirectly);
			TakesBrowserTab->FlashTab();
		}
		else
		{
			TakesBrowserTab->RequestCloseTab();
		}
	}
	else 
	{
		TakesBrowserTab = LevelEditorModule.GetLevelEditorTabManager()->TryInvokeTab(ITakeRecorderModule::TakesBrowserTabName);

		bool bAllowLockedBrowser =  true;
		bool bFocusContentBrowser = false;

		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(AssetRegistryConstants::ModuleName);
		IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

		FString TakesPath = GetTakeMetaData()->GenerateAssetPath(GetDefault<UTakeRecorderProjectSettings>()->Settings.GetTakeAssetPath());
		TakesPath = FPaths::GetPath(*TakesPath);

		while(!TakesPath.IsEmpty())
		{
			if (AssetRegistry.HasAssets(FName(*TakesPath), true))
			{
				break;
			}
			TakesPath = FPaths::GetPath(TakesPath);
		}

		TArray<FString> TakesFolder;
		TakesFolder.Push(TakesPath);
		if (AssetRegistry.HasAssets(FName(*TakesPath), true) )
		{
			FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
			ContentBrowserModule.Get().SyncBrowserToFolders(TakesFolder, bAllowLockedBrowser, bFocusContentBrowser, ITakeRecorderModule::TakesBrowserInstanceName );
		}

		TakesBrowserTab->FlashTab();
	}
}

void STakeRecorderPanel::ReconfigureExternalSettings(UObject* InExternalObject, bool bIsAdd)
{
	if (LevelSequenceTakeWidget.IsValid())
	{
		if (bIsAdd)
		{
			LevelSequenceTakeWidget->AddExternalSettingsObject(InExternalObject);
		}
		else
		{
			LevelSequenceTakeWidget->RemoveExternalSettingsObject(InExternalObject);
		}
	}
}

#undef LOCTEXT_NAMESPACE
