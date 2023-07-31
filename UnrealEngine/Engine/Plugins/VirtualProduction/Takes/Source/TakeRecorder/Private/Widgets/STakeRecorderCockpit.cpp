// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/STakeRecorderCockpit.h"
#include "Widgets/SOverlay.h"
#include "Widgets/TakeRecorderWidgetConstants.h"
#include "Widgets/STakeRecorderTabContent.h"
#include "TakesCoreBlueprintLibrary.h"
#include "TakeMetaData.h"
#include "TakeRecorderCommands.h"
#include "TakeRecorderModule.h"
#include "TakeRecorderSettings.h"
#include "TakeRecorderStyle.h"
#include "TakeRecorderSource.h"
#include "TakeRecorderSources.h"
#include "Misc/Timecode.h"
#include "Misc/FrameRate.h"
#include "Recorder/TakeRecorder.h"
#include "Recorder/TakeRecorderBlueprintLibrary.h"
#include "LevelSequence.h"
#include "Algo/Find.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"

#include "MovieSceneToolsProjectSettings.h"

// AssetRegistry includes
#include "AssetRegistry/AssetRegistryModule.h"

// TimeManagement includes
#include "FrameNumberNumericInterface.h"

// Slate includes
#include "Widgets/SBoxPanel.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/SFrameRatePicker.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"

// Style includes
#include "Styling/AppStyle.h"
#include "EditorFontGlyphs.h"

// UnrealEd includes
#include "ScopedTransaction.h"
#include "Editor/EditorEngine.h"
#include "Editor.h"
#include "Modules/ModuleManager.h"
#include "LevelEditor.h"
#include "ISettingsModule.h"
#include "Dialog/SMessageDialog.h"
#include "Subsystems/AssetEditorSubsystem.h"

#include "ISequencer.h"
#include "ILevelSequenceEditorToolkit.h"

extern UNREALED_API UEditorEngine* GEditor;

#define LOCTEXT_NAMESPACE "STakeRecorderCockpit"

STakeRecorderCockpit::~STakeRecorderCockpit()
{
	UTakeRecorder::OnRecordingInitialized().Remove(OnRecordingInitializedHandle);

	if (FAssetRegistryModule* AssetRegistryModule = FModuleManager::GetModulePtr<FAssetRegistryModule>("AssetRegistry"))
	{
		IAssetRegistry* AssetRegistry = AssetRegistryModule->TryGet();
		if (AssetRegistry)
		{
			AssetRegistry->OnFilesLoaded().Remove(OnAssetRegistryFilesLoadedHandle);
		}
	}

	if (!ensure(TransactionIndex == INDEX_NONE))
	{
		GEditor->CancelTransaction(TransactionIndex);
	}
}

void STakeRecorderCockpit::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(TakeMetaData);
	Collector.AddReferencedObject(TransientTakeMetaData);
}

struct SNonThrottledButton : SButton
{
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		FReply Reply = SButton::OnMouseButtonDown(MyGeometry, MouseEvent);
		if (Reply.IsEventHandled())
		{
			Reply.PreventThrottling();
		}
		return Reply;
	}
};

struct FDigitsTypeInterface : INumericTypeInterface<int32>
{
	virtual FString ToString(const int32& Value) const override
	{
		const UMovieSceneToolsProjectSettings* ProjectSettings = GetDefault<UMovieSceneToolsProjectSettings>();

		return FString::Printf(TEXT("%0*d"), ProjectSettings->TakeNumDigits, Value);
	}

	virtual TOptional<int32> FromString(const FString& InString, const int32& ExistingValue) override
	{
		return (int32)FCString::Atoi(*InString);
	}

	virtual int32 GetMinFractionalDigits() const override { return 0; }
	virtual int32 GetMaxFractionalDigits() const override { return 0; }

	virtual void SetMinFractionalDigits(const TAttribute<TOptional<int32>>& NewValue) override {}
	virtual void SetMaxFractionalDigits(const TAttribute<TOptional<int32>>& NewValue) override {}

	virtual bool IsCharacterValid(TCHAR InChar) const override { return true; }
};
	
void STakeRecorderCockpit::Construct(const FArguments& InArgs)
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	OnAssetRegistryFilesLoadedHandle = AssetRegistryModule.Get().OnFilesLoaded().AddSP(this, &STakeRecorderCockpit::OnAssetRegistryFilesLoaded);
	
	OnRecordingInitializedHandle = UTakeRecorder::OnRecordingInitialized().AddSP(this, &STakeRecorderCockpit::OnRecordingInitialized);

	bAutoApplyTakeNumber = true;

	TakeMetaData = nullptr;
	TransientTakeMetaData = nullptr;

	CachedTakeSlate.Empty();
	CachedTakeNumber = -1;

	LevelSequenceAttribute = InArgs._LevelSequence;

	TakeRecorderModeAttribute = InArgs._TakeRecorderMode;

	CacheMetaData();

	if (TakeMetaData && !TakeMetaData->IsLocked())
	{
		int32 NextTakeNumber = UTakesCoreBlueprintLibrary::ComputeNextTakeNumber(TakeMetaData->GetSlate());
		if (NextTakeNumber != TakeMetaData->GetTakeNumber())
		{
			TakeMetaData->SetTakeNumber(NextTakeNumber);
		}
	}

	UpdateTakeError();
	UpdateRecordError();

	CommandList = MakeShareable(new FUICommandList);
	
	DigitsTypeInterface = MakeShareable(new FDigitsTypeInterface);

	BindCommands();

	TransactionIndex = INDEX_NONE;

	int32 Column[] = { 0, 1, 2 };
	int32 Row[]    = { 0, 1, 2 };

	TSharedPtr<SOverlay> OverlayHolder;

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FTakeRecorderStyle::Get().GetBrush("TakeRecorder.Slate"))
		[

		SNew(SVerticalBox)

		// Slate, Take #, and Record Button 
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBorder)
			.BorderImage_Lambda([this]{ return Reviewing() ? FTakeRecorderStyle::Get().GetBrush("TakeRecorder.TakeRecorderReviewBorder") : FAppStyle::GetBrush("ToolPanel.DarkGroupBorder"); })
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.FillWidth(0.6)
				[
					SNew(SOverlay)

					+ SOverlay::Slot()
					.VAlign(VAlign_Top)
					.HAlign(HAlign_Left)
					.Padding(2.0f, 2.0f)
					[
						SNew(STextBlock)
						.TextStyle(FTakeRecorderStyle::Get(), "TakeRecorder.TextBox")
						.Text(LOCTEXT("SlateLabel", "SLATE"))
					]

					+ SOverlay::Slot()
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Center)
					[
						SNew(SEditableTextBox)
						.IsEnabled(this, &STakeRecorderCockpit::EditingMetaData)
						.Style(FTakeRecorderStyle::Get(), "TakeRecorder.EditableTextBox")
						.Font(FTakeRecorderStyle::Get().GetFontStyle("TakeRecorder.Cockpit.LargeText"))
						.HintText(LOCTEXT("EnterSlate_Hint", "<slate>"))
						.Justification(ETextJustify::Center)
						.SelectAllTextWhenFocused(true)
						.Text(this, &STakeRecorderCockpit::GetSlateText)
						.OnTextCommitted(this, &STakeRecorderCockpit::SetSlateText)
					]
				]

				+ SHorizontalBox::Slot()
				.FillWidth(0.4)
				[
					SNew(SOverlay)

					+ SOverlay::Slot()
					.VAlign(VAlign_Top)
					.HAlign(HAlign_Left)
					.Padding(2.0f, 2.0f)
					[
						SNew(SHorizontalBox)

						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(STextBlock)
							.TextStyle(FTakeRecorderStyle::Get(), "TakeRecorder.TextBox")
							.Text(LOCTEXT("TakeLabel", "TAKE"))
						]

						+ SHorizontalBox::Slot()
						.Padding(2.f, 0.f)
						.VAlign(VAlign_Center)
						.AutoWidth()
						[
							SNew(SButton)
							.ButtonStyle(FAppStyle::Get(), "NoBorder")
							.OnClicked(this, &STakeRecorderCockpit::OnSetNextTakeNumber)
							.ForegroundColor(FSlateColor::UseForeground())
							.Visibility(this, &STakeRecorderCockpit::GetTakeWarningVisibility)
							.Content()
							[
								SNew(STextBlock)
								.ToolTipText(this, &STakeRecorderCockpit::GetTakeWarningText)
								.Font(FAppStyle::Get().GetFontStyle("FontAwesome.8"))
								.Text(FEditorFontGlyphs::Exclamation_Triangle)
							]
						]
					]

					+ SOverlay::Slot()
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Center)
					[
						SNew(SSpinBox<int32>)
						.IsEnabled(this, &STakeRecorderCockpit::EditingMetaData)
						.ContentPadding(FMargin(8.f, 0.f))
						.Style(FTakeRecorderStyle::Get(), "TakeRecorder.TakeInput")
						.Font(FTakeRecorderStyle::Get().GetFontStyle("TakeRecorder.Cockpit.GiantText"))
						.Justification(ETextJustify::Center)
						.Value(this, &STakeRecorderCockpit::GetTakeNumber)
						.Delta(1)
						.MinValue(1)
						.MaxValue(TOptional<int32>())
						.OnBeginSliderMovement(this, &STakeRecorderCockpit::OnBeginSetTakeNumber)
						.OnValueChanged(this, &STakeRecorderCockpit::SetTakeNumber)
						.OnValueCommitted(this, &STakeRecorderCockpit::SetTakeNumber_FromCommit)
						.OnEndSliderMovement(this, &STakeRecorderCockpit::OnEndSetTakeNumber)
						.TypeInterface(DigitsTypeInterface)
					]
				]

				+SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SAssignNew(OverlayHolder,SOverlay)

					+ SOverlay::Slot()
					[
						SNew(SBox)
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						.MaxAspectRatio(1)
						.Padding(FMargin(8.0f, 8.0f, 8.0f, 8.0f))
						.Visibility_Lambda([this]() { return Reviewing() ? EVisibility::Hidden : EVisibility::Visible; })
						[
							SNew(SCheckBox)
							.Style(FTakeRecorderStyle::Get(), "TakeRecorder.RecordButton")
							.OnCheckStateChanged(this, &STakeRecorderCockpit::OnToggleRecording)
							.IsChecked(this, &STakeRecorderCockpit::IsRecording)
							.IsEnabled(this, &STakeRecorderCockpit::CanRecord)
						]
					]

					+ SOverlay::Slot()
					[
						SNew(SBox)
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						.MaxAspectRatio(1)
						.Padding(FMargin(8.0f, 8.0f, 8.0f, 8.0f))
						.Visibility_Lambda([this]() { return Reviewing() ? EVisibility::Visible : EVisibility::Hidden; })
						[
							SNew(SButton)
							.ContentPadding(TakeRecorder::ButtonPadding)
							.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
							.ToolTipText(LOCTEXT("NewRecording", "Start a new recording using this Take as a base"))
							.ForegroundColor(FSlateColor::UseForeground())
							.OnClicked(this, &STakeRecorderCockpit::NewRecordingFromThis)
							[
								SNew(SImage)
								.Image(FTakeRecorderStyle::Get().GetBrush("TakeRecorder.StartNewRecordingButton"))
							]
						]
					]

					+ SOverlay::Slot()
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.ToolTipText(this, &STakeRecorderCockpit::GetRecordErrorText)
						.Visibility(this, &STakeRecorderCockpit::GetRecordErrorVisibility)
						.Font(FAppStyle::Get().GetFontStyle("FontAwesome.9"))
						.Text(FEditorFontGlyphs::Exclamation_Triangle)
					]

					+ SOverlay::Slot()
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.ColorAndOpacity(FAppStyle::Get().GetSlateColor("InvertedForeground"))
						.Visibility(this, &STakeRecorderCockpit::GetCountdownVisibility)
						.Text(this, &STakeRecorderCockpit::GetCountdownText)
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
					.ToolTipText(LOCTEXT("RecordingOptionsTooltip", "Recording options"))
					.OnGetMenuContent(this, &STakeRecorderCockpit::OnRecordingOptionsMenu)
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

		// Timestamp, Duration, Description and Remaining Metadata
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBorder)
			.BorderImage(FTakeRecorderStyle::Get().GetBrush("TakeRecorder.Slate.BorderImage"))
			.BorderBackgroundColor(FTakeRecorderStyle::Get().GetColor("TakeRecorder.Slate.BorderColor"))
			[
				SNew(SVerticalBox)

				+SVerticalBox::Slot()
				.Padding(8, 4, 0, 4)
				.AutoHeight()
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Font(FTakeRecorderStyle::Get().GetFontStyle("TakeRecorder.Cockpit.SmallText"))
						.ColorAndOpacity(FSlateColor::UseSubduedForeground())
						.Text(this, &STakeRecorderCockpit::GetTimestampText)
						.ToolTipText(this, &STakeRecorderCockpit::GetTimestampTooltipText)
					]

					+ SHorizontalBox::Slot()
					[
						SNew(SSpacer)
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Font(FTakeRecorderStyle::Get().GetFontStyle("TakeRecorder.Cockpit.MediumText"))
						.ColorAndOpacity(FSlateColor::UseSubduedForeground())
						.Justification(ETextJustify::Right)
						.Text(this, &STakeRecorderCockpit::GetTimecodeText)
						.ToolTipText(LOCTEXT("Timecode", "The current timecode"))
					]

					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SNonThrottledButton)
						.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
						.ToolTipText(LOCTEXT("AddMarkedFrame", "Click to add a marked frame while recording"))
						.IsEnabled_Lambda([this]() { return IsRecording() == ECheckBoxState::Checked; })
						.OnClicked(this, &STakeRecorderCockpit::OnAddMarkedFrame)
						.ForegroundColor(FSlateColor::UseForeground())
						[
							SNew(SImage)
							.Image(FTakeRecorderStyle::Get().GetBrush("TakeRecorder.MarkFrame"))
						]
					]
				]

				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(8, 4)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					[
						SNew(SSpacer)
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SComboButton)
						.ButtonStyle(FAppStyle::Get(), "NoBorder")
						.OnGetMenuContent(this, &STakeRecorderCockpit::OnCreateMenu)
						.ForegroundColor(FSlateColor::UseForeground())
						.ButtonContent()
						[
							SNew(STextBlock)
							.ColorAndOpacity(FSlateColor::UseSubduedForeground())
							.Font(FTakeRecorderStyle::Get().GetFontStyle("TakeRecorder.Cockpit.SmallText"))
							.Text(this, &STakeRecorderCockpit::GetFrameRateText)
							.ToolTipText(this, &STakeRecorderCockpit::GetFrameRateTooltipText)
						]
					]
				]

				+SVerticalBox::Slot()
				.Padding(8, 0, 8, 8)
				.AutoHeight()
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					[
						SNew(SEditableTextBox)
						.IsEnabled(this, &STakeRecorderCockpit::EditingMetaData)
						.Style(FTakeRecorderStyle::Get(), "TakeRecorder.EditableTextBox")
						.Font(FTakeRecorderStyle::Get().GetFontStyle("TakeRecorder.Cockpit.SmallText"))
						.SelectAllTextWhenFocused(true)
						.HintText(LOCTEXT("EnterSlateDescription_Hint", "<description>"))
						.Text(this, &STakeRecorderCockpit::GetUserDescriptionText)
						.OnTextCommitted(this, &STakeRecorderCockpit::SetUserDescriptionText)
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SSpinBox<float>)
						.ToolTipText(LOCTEXT("EngineTimeDilation", "Recording speed"))
						.Style(&FAppStyle::GetWidgetStyle<FSpinBoxStyle>("Sequencer.HyperlinkSpinBox"))
						.OnValueChanged(this, &STakeRecorderCockpit::SetEngineTimeDilation)
						.OnValueCommitted_Lambda([=](float InEngineTimeDilation, ETextCommit::Type) { SetEngineTimeDilation(InEngineTimeDilation); })
						.MinValue(TOptional<float>())
						.MaxValue(TOptional<float>())
						.Value(this, &STakeRecorderCockpit::GetEngineTimeDilation)
						.Delta(0.5f)
					]

					+ SHorizontalBox::Slot()
					.Padding(2, 0, 0, 2)
					.VAlign(VAlign_Bottom)
					.AutoWidth()
					[
						SNew(STextBlock)
						.ColorAndOpacity(FSlateColor::UseSubduedForeground())
						.Font(FTakeRecorderStyle::Get().GetFontStyle("TakeRecorder.Cockpit.SmallText"))
						.TextStyle(FTakeRecorderStyle::Get(), "TakeRecorder.TextBox")
						.Text(LOCTEXT("EngineTimeDilationLabel", "x"))
					]
				]
			]
		]
		]
	];

	ITakeRecorderModule& TakeRecorderModule = FModuleManager::Get().LoadModuleChecked<ITakeRecorderModule>("TakeRecorder");
	TArray<TSharedRef<SWidget>> OutExtensions;
	TakeRecorderModule.GetRecordButtonExtensionGenerators().Broadcast(OutExtensions);
	for (const TSharedRef<SWidget>& Widget : OutExtensions)
	{
		OverlayHolder->AddSlot()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				Widget
			];
	}
}

bool STakeRecorderCockpit::CanStartRecording(FText& OutErrorText) const
{
	bool bCanRecord = CanRecord();
	if (!bCanRecord)
	{
		OutErrorText = RecordErrorText;
	}
	return bCanRecord;
}

FText STakeRecorderCockpit::GetTakeWarningText() const
{
	return TakeErrorText;
}

EVisibility STakeRecorderCockpit::GetTakeWarningVisibility() const
{
	return TakeErrorText.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible;
}

FText STakeRecorderCockpit::GetRecordErrorText() const
{
	return RecordErrorText;
}

EVisibility STakeRecorderCockpit::GetRecordErrorVisibility() const
{
	return RecordErrorText.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible;
}

void STakeRecorderCockpit::UpdateRecordError()
{
	RecordErrorText = FText();
	if (Reviewing())
	{
		// When take meta-data is locked, we cannot record until we hit the "Start a new recording using this Take as a base"
		// For this reason, we don't show any error information because we can always start a new recording from any take
		return;
	}

	ULevelSequence* Sequence = LevelSequenceAttribute.Get();
	if (!Sequence)
	{
		RecordErrorText = LOCTEXT("ErrorWidget_NoSequence", "There is no sequence to record from. Please re-open Take Recorder.");
		return;
	}

	if (!Sequence->HasAnyFlags(RF_Transient) && TakeRecorderModeAttribute.Get() != ETakeRecorderMode::RecordIntoSequence)
	{
		RecordErrorText = FText();
		return;
	}

	UTakeRecorderSources*                  SourcesContainer = Sequence->FindMetaData<UTakeRecorderSources>();
	TArrayView<UTakeRecorderSource* const> SourcesArray     = SourcesContainer ? SourcesContainer->GetSources() : TArrayView<UTakeRecorderSource* const>();
	UTakeRecorderSource* const* Source = Algo::FindByPredicate(SourcesArray, [](const UTakeRecorderSource* Source)
	{
		return Source->bEnabled && Source->IsValid();
	});
	if (!Source)
	{
		RecordErrorText = LOCTEXT("ErrorWidget_NoSources", "There are no currently enabled or valid sources to record from. Please add some above before recording.");
		return;
	}

	if (TakeMetaData->GetSlate().IsEmpty())
	{
		RecordErrorText = LOCTEXT("ErrorWidget_NoSlate", "You must enter a slate to begin recording.");
		return;
	}
	FString PackageName = TakeMetaData->GenerateAssetPath(GetDefault<UTakeRecorderProjectSettings>()->Settings.GetTakeAssetPath());
	FText OutReason;
	if (!FPackageName::IsValidLongPackageName(PackageName, false, &OutReason))
	{
		RecordErrorText = FText::Format(LOCTEXT("ErrorWidget_InvalidPath", "{0} is not a valid asset path. {1}"), FText::FromString(PackageName), OutReason);
		return;
	}

	const int32 MaxLength = 260;

	if (PackageName.Len() > MaxLength)
	{
		RecordErrorText = FText::Format(LOCTEXT("ErrorWidget_TooLong", "The path to the asset is too long ({0} characters), the maximum is {1}.\nPlease choose a shorter name for the slate or create it in a shallower folder structure with shorter folder names."), FText::AsNumber(PackageName.Len()), FText::AsNumber(MaxLength));
		return;
	}
	ITakeRecorderModule& TakeRecorderModule = FModuleManager::Get().LoadModuleChecked<ITakeRecorderModule>("TakeRecorder");
	TakeRecorderModule.GetRecordErrorCheckGenerator().Broadcast(RecordErrorText);
}

void STakeRecorderCockpit::UpdateTakeError()
{
	TakeErrorText = FText();

	TArray<FAssetData> DuplicateTakes = UTakesCoreBlueprintLibrary::FindTakes(TakeMetaData->GetSlate(), TakeMetaData->GetTakeNumber());

	// If there's only a single one, and it's the one that we're looking at directly, don't show the error
	if (DuplicateTakes.Num() == 1 && DuplicateTakes[0].IsValid())
	{
		ULevelSequence* AlreadyLoaded = FindObject<ULevelSequence>(nullptr, *DuplicateTakes[0].GetObjectPathString());
		if (AlreadyLoaded && AlreadyLoaded->FindMetaData<UTakeMetaData>() == TakeMetaData)
		{
			return;
		}
	}

	if (DuplicateTakes.Num() > 0)
	{
		FTextBuilder TextBuilder;
		TextBuilder.AppendLineFormat(
			LOCTEXT("DuplicateTakeNumber_1", "The following Level {0}|plural(one=Sequence, other=Sequences) {0}|plural(one=was, other=were) also recorded with take {1} of {2}"),
			DuplicateTakes.Num(),
			FText::AsNumber(TakeMetaData->GetTakeNumber()),
			FText::FromString(TakeMetaData->GetSlate())
		);

		for (const FAssetData& Asset : DuplicateTakes)
		{
			TextBuilder.AppendLine(FText::FromName(Asset.PackageName));
		}

		TextBuilder.AppendLine(LOCTEXT("GetNextAvailableTakeNumber", "Click to get the next available take number."));
		TakeErrorText = TextBuilder.ToText();
	}
}

EVisibility STakeRecorderCockpit::GetCountdownVisibility() const
{
	const UTakeRecorder* CurrentRecording = UTakeRecorder::GetActiveRecorder();
	const bool           bIsCountingDown  = CurrentRecording && CurrentRecording->GetState() == ETakeRecorderState::CountingDown;

	return bIsCountingDown ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
}

FText STakeRecorderCockpit::GetCountdownText() const
{
	const UTakeRecorder* CurrentRecording = UTakeRecorder::GetActiveRecorder();
	const bool           bIsCountingDown  = CurrentRecording && CurrentRecording->GetState() == ETakeRecorderState::CountingDown;

	return bIsCountingDown ? FText::AsNumber(FMath::CeilToInt(CurrentRecording->GetCountdownSeconds())) : FText();
}

TSharedRef<SWidget> STakeRecorderCockpit::OnRecordingOptionsMenu()
{
	FMenuBuilder MenuBuilder(true, nullptr);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("CancelRecording_Text", "Cancel Recording"),
		LOCTEXT("CancelRecording_Tip", "Cancel the current recording, deleting any assets and resetting the take number"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP(this, &STakeRecorderCockpit::CancelRecording),
			FCanExecuteAction::CreateLambda([this] { return Recording(); })
		)
	);

	return MenuBuilder.MakeWidget();
}

void STakeRecorderCockpit::CacheMetaData()
{
	UTakeMetaData* NewMetaDataThisTick = nullptr;

	if (ULevelSequence* LevelSequence = LevelSequenceAttribute.Get())
	{
		NewMetaDataThisTick = LevelSequence->FindMetaData<UTakeMetaData>();
	}

	// If it's null we use the transient meta-data
	if (!NewMetaDataThisTick)
	{
		// if the transient meta-data doesn't exist, create it now
		if (!TransientTakeMetaData)
		{
			TransientTakeMetaData = UTakeMetaData::CreateFromDefaults(GetTransientPackage(), NAME_None);
			TransientTakeMetaData->SetFlags(RF_Transactional | RF_Transient);

			FString DefaultSlate = GetDefault<UTakeRecorderProjectSettings>()->Settings.DefaultSlate;
			if (TransientTakeMetaData->GetSlate() != DefaultSlate)
			{
				TransientTakeMetaData->SetSlate(DefaultSlate, false);
			}

			// Compute the correct starting take number
			int32 NextTakeNumber = UTakesCoreBlueprintLibrary::ComputeNextTakeNumber(TransientTakeMetaData->GetSlate());
			if (TransientTakeMetaData->GetTakeNumber() != NextTakeNumber)
			{
				TransientTakeMetaData->SetTakeNumber(NextTakeNumber, false);
			}
		}

		NewMetaDataThisTick = TransientTakeMetaData;
	}

	check(NewMetaDataThisTick);
	if (NewMetaDataThisTick != TakeMetaData)
	{
		TakeMetaData = NewMetaDataThisTick;
	}

	if (TakeMetaData->GetSlate() != CachedTakeSlate || TakeMetaData->GetTakeNumber() != CachedTakeNumber)
	{
		CachedTakeNumber = TakeMetaData->GetTakeNumber();
		CachedTakeSlate = TakeMetaData->GetSlate();

		// Previously, the take error would be updated in Tick(), but the asset registry can be slow, 
		// so it should be sufficient to update it only when the slate changes.
		UpdateTakeError();
	}

	//Set MovieScene Display Rate to the Preset Frame Rate.
	ULevelSequence* Sequence = LevelSequenceAttribute.Get();
	UMovieScene*    MovieScene = Sequence ? Sequence->GetMovieScene() : nullptr;
	if (MovieScene)
	{
		MovieScene->SetDisplayRate(TakeMetaData->GetFrameRate());
	}

	check(TakeMetaData);
}

void STakeRecorderCockpit::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	Refresh();
}

void STakeRecorderCockpit::Refresh()
{
	CacheMetaData();
	UpdateRecordError();
}

FText STakeRecorderCockpit::GetSlateText() const
{
	return FText::FromString(TakeMetaData->GetSlate());
}

FText STakeRecorderCockpit::GetTimecodeText() const
{
	return FText::FromString(FApp::GetTimecode().ToString());
}

FText STakeRecorderCockpit::GetUserDescriptionText() const
{
	return FText::FromString(TakeMetaData->GetDescription());
}

FText STakeRecorderCockpit::GetTimestampText() const
{
	// If not recorded, return current time
	if (TakeMetaData->GetTimestamp() == FDateTime(0))
	{
		return FText::AsDateTime(FDateTime::UtcNow());
	}
	else
	{
		return FText::AsDateTime(TakeMetaData->GetTimestamp());
	}
}

FText STakeRecorderCockpit::GetTimestampTooltipText() const
{
	// If not recorded, return current time
	if (TakeMetaData->GetTimestamp() == FDateTime(0))
	{
		return LOCTEXT("CurrentTimestamp", "The current date/time");
	}
	else
	{
		return LOCTEXT("Timestamp", "The date/time this recording was created at");
	}
}

void STakeRecorderCockpit::SetFrameRate(FFrameRate InFrameRate, bool bFromTimecode)
{
	if (TakeMetaData)
	{
		TakeMetaData->SetFrameRateFromTimecode(bFromTimecode);
		TakeMetaData->SetFrameRate(InFrameRate);
	}
	ULevelSequence* Sequence = LevelSequenceAttribute.Get();
	UMovieScene*    MovieScene = Sequence ? Sequence->GetMovieScene() : nullptr;
	if (MovieScene)
	{
		MovieScene->SetDisplayRate(InFrameRate);
	}
}

bool STakeRecorderCockpit::IsSameFrameRate(FFrameRate InFrameRate) const
{
	return (InFrameRate == GetFrameRate());
}

FFrameRate STakeRecorderCockpit::GetFrameRate() const
{
	return TakeMetaData->GetFrameRate();
}

FText STakeRecorderCockpit::GetFrameRateText() const
{
	return GetFrameRate().ToPrettyText();
}

FText STakeRecorderCockpit::GetFrameRateTooltipText() const
{
	return LOCTEXT("ProjectFrameRate", "The project timecode frame rate. The resulting recorded sequence will be at this frame rate.");
}

bool STakeRecorderCockpit::IsFrameRateCompatible(FFrameRate InFrameRate) const
{
	ULevelSequence* Sequence   = LevelSequenceAttribute.Get();
	UMovieScene*    MovieScene = Sequence ? Sequence->GetMovieScene() : nullptr;

	return MovieScene && InFrameRate.IsMultipleOf(MovieScene->GetTickResolution());
}

bool STakeRecorderCockpit::IsSetFromTimecode() const
{
	return TakeMetaData->GetFrameRateFromTimecode();
}

void STakeRecorderCockpit::SetSlateText(const FText& InNewText, ETextCommit::Type InCommitType)
{
	if (TakeMetaData->GetSlate() != InNewText.ToString())
	{
		FScopedTransaction Transaction(LOCTEXT("SetSlate_Transaction", "Set Take Slate"));
		TakeMetaData->Modify();

		TakeMetaData->SetSlate(InNewText.ToString());

		// Compute the correct starting take number
		int32 NextTakeNumber = UTakesCoreBlueprintLibrary::ComputeNextTakeNumber(TakeMetaData->GetSlate());

		if (NextTakeNumber != TakeMetaData->GetTakeNumber())
		{
			TakeMetaData->SetTakeNumber(NextTakeNumber);
		}
	}
}

void STakeRecorderCockpit::SetUserDescriptionText(const FText& InNewText, ETextCommit::Type InCommitType)
{
	if (TakeMetaData->GetDescription() != InNewText.ToString())
	{
		FScopedTransaction Transaction(LOCTEXT("SetDescription_Transaction", "Set Description"));
		TakeMetaData->Modify();

		TakeMetaData->SetDescription(InNewText.ToString());
	}
}

int32 STakeRecorderCockpit::GetTakeNumber() const
{
	return TakeMetaData->GetTakeNumber();
}

FReply STakeRecorderCockpit::OnSetNextTakeNumber()
{
	int32 NextTakeNumber = UTakesCoreBlueprintLibrary::ComputeNextTakeNumber(TakeMetaData->GetSlate());

	if (TakeMetaData->GetTakeNumber() != NextTakeNumber)
	{
		FScopedTransaction Transaction(LOCTEXT("SetNextTakeNumber_Transaction", "Set Next Take Number"));

		TakeMetaData->Modify();
		TakeMetaData->SetTakeNumber(NextTakeNumber);
	}

	return FReply::Handled();
}

void STakeRecorderCockpit::OnBeginSetTakeNumber()
{
	const bool bIsInPIEOrSimulate = GEditor->PlayWorld != NULL || GEditor->bIsSimulatingInEditor;

	if (!bIsInPIEOrSimulate)
	{
		check(TransactionIndex == INDEX_NONE);
	}

	TransactionIndex = GEditor->BeginTransaction(nullptr, LOCTEXT("SetTakeNumber_Transaction", "Set Take Number"), nullptr);
	TakeMetaData->Modify();
}

void STakeRecorderCockpit::SetTakeNumber(int32 InNewTakeNumber)
{
	const bool bIsInPIEOrSimulate = GEditor->PlayWorld != NULL || GEditor->bIsSimulatingInEditor;

	if (TransactionIndex != INDEX_NONE || bIsInPIEOrSimulate)
	{
		TakeMetaData->SetTakeNumber(InNewTakeNumber, false);
		bAutoApplyTakeNumber = false;
	}
}

void STakeRecorderCockpit::SetTakeNumber_FromCommit(int32 InNewTakeNumber, ETextCommit::Type InCommitType)
{
	const bool bIsInPIEOrSimulate = GEditor->PlayWorld != NULL || GEditor->bIsSimulatingInEditor;

	if (TransactionIndex == INDEX_NONE && !bIsInPIEOrSimulate)
	{
		if (TakeMetaData->GetTakeNumber() != InNewTakeNumber)
		{
			OnBeginSetTakeNumber();
			OnEndSetTakeNumber(InNewTakeNumber);
		}
	}
	else if (TakeMetaData->GetTakeNumber() != InNewTakeNumber)
	{
		TakeMetaData->SetTakeNumber(InNewTakeNumber);
	}

	bAutoApplyTakeNumber = false;
}

void STakeRecorderCockpit::OnEndSetTakeNumber(int32 InFinalValue)
{
	const bool bIsInPIEOrSimulate = GEditor->PlayWorld != NULL || GEditor->bIsSimulatingInEditor;

	if (!bIsInPIEOrSimulate)
	{
		check(TransactionIndex != INDEX_NONE);
	}

	TakeMetaData->SetTakeNumber(InFinalValue);

	GEditor->EndTransaction();
	TransactionIndex = INDEX_NONE;
}

float STakeRecorderCockpit::GetEngineTimeDilation() const
{
	return GetDefault<UTakeRecorderUserSettings>()->Settings.EngineTimeDilation;
}

void STakeRecorderCockpit::SetEngineTimeDilation(float InEngineTimeDilation)
{
	GetMutableDefault<UTakeRecorderUserSettings>()->Settings.EngineTimeDilation = InEngineTimeDilation;
}

FReply STakeRecorderCockpit::OnAddMarkedFrame()
{
	if (UTakeRecorderBlueprintLibrary::IsRecording())
	{
		FFrameRate FrameRate = TakeMetaData->GetFrameRate();

		FTimespan RecordingDuration = FDateTime::UtcNow() - TakeMetaData->GetTimestamp();

		FFrameNumber ElapsedFrame = FFrameNumber(static_cast<int32>(FrameRate.AsDecimal() * RecordingDuration.GetTotalSeconds()));
		
		ULevelSequence* LevelSequence = LevelSequenceAttribute.Get();
		if (!LevelSequence)
		{
			return FReply::Handled();
		}

		UMovieScene* MovieScene = LevelSequence->GetMovieScene();
		if (!MovieScene)
		{
			return FReply::Handled();
		}

		FFrameRate DisplayRate = MovieScene->GetDisplayRate();
		FFrameRate TickResolution = MovieScene->GetTickResolution();

		FMovieSceneMarkedFrame MarkedFrame;

		UTakeRecorderSources* Sources = LevelSequence->FindMetaData<UTakeRecorderSources>();
		if (Sources && Sources->GetSettings().bStartAtCurrentTimecode)
		{
			MarkedFrame.FrameNumber = FFrameRate::TransformTime(FFrameTime(FApp::GetTimecode().ToFrameNumber(DisplayRate)), DisplayRate, TickResolution).FloorToFrame();
		}
		else
		{
			MarkedFrame.FrameNumber = ConvertFrameTime(ElapsedFrame, DisplayRate, TickResolution).CeilToFrame();
		}
		
		int32 MarkedFrameIndex = MovieScene->AddMarkedFrame(MarkedFrame);
		UTakeRecorderBlueprintLibrary::OnTakeRecorderMarkedFrameAdded(MovieScene->GetMarkedFrames()[MarkedFrameIndex]);
	}

	return FReply::Handled();
}

bool STakeRecorderCockpit::Reviewing() const 
{
	return bool(!Recording() && (TakeMetaData->Recorded() && TakeRecorderModeAttribute.Get() != ETakeRecorderMode::RecordIntoSequence));
}

bool STakeRecorderCockpit::Recording() const
{
	return UTakeRecorderBlueprintLibrary::GetActiveRecorder() ? true : false;
}

ECheckBoxState STakeRecorderCockpit::IsRecording() const
{
	return Recording() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

bool STakeRecorderCockpit::CanRecord() const
{
	return RecordErrorText.IsEmpty();
}

bool STakeRecorderCockpit::IsLocked() const 
{
	if (TakeMetaData != nullptr)
	{
		return TakeMetaData->IsLocked();
	}
	return false;
}

void STakeRecorderCockpit::OnToggleRecording(ECheckBoxState)
{
	ULevelSequence*       LevelSequence = LevelSequenceAttribute.Get();
	UTakeRecorderSources* Sources = LevelSequence ? LevelSequence->FindMetaData<UTakeRecorderSources>() : nullptr;

	UTakeRecorder* CurrentRecording = UTakeRecorder::GetActiveRecorder();
	if (CurrentRecording)
	{
		StopRecording();
	}
	else if (LevelSequence && Sources)
	{
		StartRecording();
	}
}

void STakeRecorderCockpit::StopRecording()
{
	UTakeRecorder* CurrentRecording = UTakeRecorder::GetActiveRecorder();
	if (CurrentRecording)
	{
		CurrentRecording->Stop();
	}
}

void STakeRecorderCockpit::CancelRecording()
{
	UTakeRecorder* CurrentRecording = UTakeRecorder::GetActiveRecorder();
	if (CurrentRecording)
	{
		TSharedRef<SMessageDialog> ConfirmDialog = SNew(SMessageDialog)
			.Title(FText(LOCTEXT("ConfirmCancelRecordingTitle", "Cancel Recording?")))
			.Message(LOCTEXT("ConfirmCancelRecording", "Are you sure you want to cancel the current recording?"))
			.Buttons({
				SCustomDialog::FButton(LOCTEXT("Yes", "Yes"), FSimpleDelegate::CreateLambda([WeakRecording = TWeakObjectPtr<UTakeRecorder>(CurrentRecording)]()
					{ 
						if (WeakRecording.IsValid())
						{
							WeakRecording.Get()->Cancel(); 
						}
					})),
				SCustomDialog::FButton(LOCTEXT("No", "No"))
			});

		// Non modal so that the recording continues to update
		ConfirmDialog->Show();
	}
}

void STakeRecorderCockpit::StartRecording()
{
	static bool bStartedRecording = false;

	if (bStartedRecording)
	{
		return;
	}

	TGuardValue<bool> ReentrantGuard(bStartedRecording, true);

	ULevelSequence*       LevelSequence = LevelSequenceAttribute.Get();
	UTakeRecorderSources* Sources = LevelSequence ? LevelSequence->FindMetaData<UTakeRecorderSources>() : nullptr;
	
	if (LevelSequence && Sources)
	{
		FTakeRecorderParameters Parameters;
		Parameters.User    = GetDefault<UTakeRecorderUserSettings>()->Settings;
		Parameters.Project = GetDefault<UTakeRecorderProjectSettings>()->Settings;
		Parameters.TakeRecorderMode = TakeRecorderModeAttribute.Get();
		Parameters.StartFrame = LevelSequence->GetMovieScene()->GetPlaybackRange().GetLowerBoundValue();

		FText ErrorText = LOCTEXT("UnknownError", "An unknown error occurred when trying to start recording");

		IAssetEditorInstance* AssetEditor = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->FindEditorForAsset(LevelSequence, false);
		ILevelSequenceEditorToolkit* LevelSequenceEditor = static_cast<ILevelSequenceEditorToolkit*>(AssetEditor);

		if (LevelSequenceEditor && LevelSequenceEditor->GetSequencer())
		{
			// If not resetting the playhead, store the current time as the start frame for recording. 
			// This will ultimately be the start of the playback range and the recording will begin from that time.
			if (!Parameters.User.bResetPlayhead)
			{
				Parameters.StartFrame = LevelSequenceEditor->GetSequencer()->GetLocalTime().Time.FrameNumber;
			}
		}

		UTakeRecorder* NewRecorder = NewObject<UTakeRecorder>(GetTransientPackage(), NAME_None, RF_Transient);

		if (!NewRecorder->Initialize(LevelSequence, Sources, TakeMetaData, Parameters, &ErrorText))
		{
			if (ensure(!ErrorText.IsEmpty()))
			{
				FNotificationInfo Info(ErrorText);
				Info.ExpireDuration = 5.0f;
				FSlateNotificationManager::Get().AddNotification(Info)->SetCompletionState(SNotificationItem::CS_Fail);
			}
		}
	}
}

FReply STakeRecorderCockpit::NewRecordingFromThis()
{
	ULevelSequence* Sequence = LevelSequenceAttribute.Get();
	if (!Sequence)
	{
		return FReply::Unhandled();
	}

	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
	TSharedPtr<SDockTab> DockTab = LevelEditorModule.GetLevelEditorTabManager()->TryInvokeTab(ITakeRecorderModule::TakeRecorderTabName);
	if (DockTab.IsValid())
	{
		TSharedRef<STakeRecorderTabContent> TabContent = StaticCastSharedRef<STakeRecorderTabContent>(DockTab->GetContent());
		TabContent->SetupForRecording(Sequence);
	}

	return FReply::Handled();
}

void STakeRecorderCockpit::OnAssetRegistryFilesLoaded()
{
	if (bAutoApplyTakeNumber && TransientTakeMetaData)
	{
		int32 NextTakeNumber = UTakesCoreBlueprintLibrary::ComputeNextTakeNumber(TransientTakeMetaData->GetSlate());
		TransientTakeMetaData->SetTakeNumber(NextTakeNumber);
	}
}

void STakeRecorderCockpit::OnRecordingInitialized(UTakeRecorder* Recorder)
{
	// Recache the meta-data here since we know that the sequence has probably changed as a result of the recording being started
	CacheMetaData();

	OnRecordingFinishedHandle = Recorder->OnRecordingFinished().AddSP(this, &STakeRecorderCockpit::OnRecordingFinished);
}

void STakeRecorderCockpit::OnRecordingFinished(UTakeRecorder* Recorder)
{
	if (TransientTakeMetaData)
	{
		// Increment the transient take meta data if necessary
		int32 NextTakeNumber = UTakesCoreBlueprintLibrary::ComputeNextTakeNumber(TransientTakeMetaData->GetSlate());

		if (TransientTakeMetaData->GetTakeNumber() != NextTakeNumber)
		{
			TransientTakeMetaData->SetTakeNumber(NextTakeNumber);
		}

		bAutoApplyTakeNumber = true;
	}

	Recorder->OnRecordingFinished().Remove(OnRecordingFinishedHandle);
}

void STakeRecorderCockpit::BindCommands()
{
	CommandList->MapAction(FTakeRecorderCommands::Get().StartRecording,
		FExecuteAction::CreateSP(this, &STakeRecorderCockpit::StartRecording));

	CommandList->MapAction(FTakeRecorderCommands::Get().StopRecording,
		FExecuteAction::CreateSP(this, &STakeRecorderCockpit::StopRecording));

	// Append to level editor module so that shortcuts are accessible in level editor
	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
	LevelEditorModule.GetGlobalLevelEditorActions()->Append(CommandList.ToSharedRef());
}

void STakeRecorderCockpit::OnToggleEditPreviousRecording(ECheckBoxState CheckState)
{
	if (Reviewing())
	{
		TakeMetaData->IsLocked() ? TakeMetaData->Unlock() : TakeMetaData->Lock();
	}	
}

bool STakeRecorderCockpit::EditingMetaData() const
{
	return (!Reviewing() || !TakeMetaData->IsLocked());
}

TSharedRef<SWidget> STakeRecorderCockpit::MakeLockButton()
{
	return SNew(SCheckBox)
	.Style(FAppStyle::Get(), "ToggleButtonCheckbox")
	.Padding(TakeRecorder::ButtonPadding)
	.ToolTipText(LOCTEXT("Modify Slate", "Unlock to modify the slate information for this prior recording."))
	.IsChecked_Lambda([this]() { return TakeMetaData->IsLocked() ? ECheckBoxState::Unchecked: ECheckBoxState::Checked; } )
	.OnCheckStateChanged(this, &STakeRecorderCockpit::OnToggleEditPreviousRecording)
	.Visibility_Lambda([this]() { return Reviewing() ? EVisibility::Visible : EVisibility::Collapsed; })
	[
		SNew(STextBlock)
		.Justification(ETextJustify::Center)
		.Font(FAppStyle::Get().GetFontStyle("FontAwesome.14"))
		.Text_Lambda([this]() { return TakeMetaData->IsLocked() ? FEditorFontGlyphs::Lock : FEditorFontGlyphs::Unlock; } )
	];
}

TSharedRef<SWidget> STakeRecorderCockpit::OnCreateMenu()
{
	ULevelSequence* Sequence = LevelSequenceAttribute.Get();
	if (!Sequence || !Sequence->GetMovieScene())
	{
		return SNullWidget::NullWidget;
	}
	UMovieScene* MovieScene = Sequence->GetMovieScene();

	
	FMenuBuilder MenuBuilder(true, nullptr);

	FFrameRate TickResolution = MovieScene->GetTickResolution();

	TArray<FCommonFrameRateInfo> CompatibleRates;
	for (const FCommonFrameRateInfo& Info : FCommonFrameRates::GetAll())
	{
		if (Info.FrameRate.IsMultipleOf(TickResolution))
		{
			CompatibleRates.Add(Info);
		}
	}

	CompatibleRates.Sort(
		[=](const FCommonFrameRateInfo& A, const FCommonFrameRateInfo& B)
	{
		return A.FrameRate.AsDecimal() < B.FrameRate.AsDecimal();
	}
	);

	MenuBuilder.BeginSection(NAME_None, LOCTEXT("RecommendedRates", "Sequence Display Rate"));
	{
		for (const FCommonFrameRateInfo& Info : CompatibleRates)
		{
			MenuBuilder.AddMenuEntry(
				Info.DisplayName,
				Info.Description,
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateSP(this, &STakeRecorderCockpit::SetFrameRate, Info.FrameRate,false),
					FCanExecuteAction(),
					FIsActionChecked::CreateSP(this, &STakeRecorderCockpit::IsSameFrameRate, Info.FrameRate)
				),
				NAME_None,
				EUserInterfaceActionType::RadioButton
			);

		}
	}
	MenuBuilder.EndSection();

	MenuBuilder.AddMenuSeparator();
	FFrameRate TimecodeFrameRate = FApp::GetTimecodeFrameRate();
	FText DisplayName = FText::Format(LOCTEXT("TimecodeFrameRate", "Timecode ({0})"), TimecodeFrameRate.ToPrettyText());

	MenuBuilder.AddMenuEntry(
		DisplayName,
		DisplayName,
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP(this, &STakeRecorderCockpit::SetFrameRate, TimecodeFrameRate,true),
			FCanExecuteAction(),
			FIsActionChecked::CreateSP(this, &STakeRecorderCockpit::IsSetFromTimecode)
		),
		NAME_None,
		EUserInterfaceActionType::RadioButton
	);
	return MenuBuilder.MakeWidget();
}

#undef LOCTEXT_NAMESPACE
