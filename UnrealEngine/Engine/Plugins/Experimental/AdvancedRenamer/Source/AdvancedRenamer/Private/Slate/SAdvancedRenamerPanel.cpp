// Copyright Epic Games, Inc. All Rights Reserved.

#include "Slate/SAdvancedRenamerPanel.h"
#include "AdvancedRenamerModule.h"
#include "AdvancedRenamerStyle.h"
#include "EngineAnalytics.h"
#include "Framework/Commands/GenericCommands.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Internationalization/Regex.h"
#include "Providers/IAdvancedRenamerProvider.h"
#include "ScopedTransaction.h"
#include "Styling/AppStyle.h"
#include "Styling/StyleColors.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SCanvas.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "SAdvancedRenamerPanel"

FName FAdvancedRenamerPreviewListItem::OriginalNameColumnName = "OriginalName";
FName FAdvancedRenamerPreviewListItem::NewNameColumnName = "NewName";

namespace UE::AdvancedRenamer::Private
{
	const FVector2D WindowSize = {600.f, 500.f};
	const FSlateFontInfo TitleFont = FCoreStyle::GetDefaultFontStyle("Regular", 12);
	const FSlateFontInfo RegularFont = FCoreStyle::GetDefaultFontStyle("Regular", 10);
	const float LeftBlockWidth = 304.f;
	const FVector2D TitleSize = {150.f, 20.f};
	const float TitleOffsetY = 25.f;
	const FVector2D CheckboxOffset = {0.f, 2.f};
	const FVector2D CheckboxSize = {18.f, 18.f};
	const FVector2D RadioOffset = {1.f, 2.f};
	const float LabelStart = 25.f;
	const float LabelOffsetY = 3.f;
	const FVector2D LabelSize = {125.f, 18.f};
	const float EntryStart = 145.f;
	const FVector2D EntrySize = {155.f, 21.f};
	const float LineHeight = 26.f;
	const FVector2D SeparatorSize = {32.f, 21.f};
	const FVector2D SpinSize1 = {32.f, 21.f};
	const FVector2D SpinSize2 = {39.f, 21.f};
	const FVector2D SpinSize3 = {46.f, 21.f};
	const float RightBlockOffsetX = 314.f;
	const FVector2D RightBlockSize = {281.f, 490.f};
	const FVector2D ListViewSize = {277.f, 431.f};
	const float ListLineHeight = 15.f;
	const float ApplyButtonHeight = 25.f;
}

void SAdvancedRenamerPreviewListRow::Construct(const FArguments& InArgs, TSharedPtr<SAdvancedRenamerPanel> InRenamePanel, 
	const TSharedRef<STableViewBase>& InOwnerTableView, FObjectRenamePreviewListItemPtr InRowItem)
{
	RenamePanel = InRenamePanel;
	RowItem = InRowItem;

	SMultiColumnTableRow<FObjectRenamePreviewListItemPtr>::Construct(FSuperRowType::FArguments(), InOwnerTableView);
	SetBorderImage(TAttribute<const FSlateBrush*>(this, &SAdvancedRenamerPreviewListRow::GetBorder));
}

TSharedRef<SWidget> SAdvancedRenamerPreviewListRow::GenerateWidgetForColumn(const FName& ColumnName)
{
	if (ColumnName != FAdvancedRenamerPreviewListItem::OriginalNameColumnName
		&& ColumnName != FAdvancedRenamerPreviewListItem::NewNameColumnName)
	{
		return SNullWidget::NullWidget;
	}

	TSharedPtr<SAdvancedRenamerPanel> RenamePanelSP = RenamePanel.Pin();

	if (!RenamePanelSP.IsValid())
	{
		return SNullWidget::NullWidget;
	}

	FObjectRenamePreviewListItemPtr RowItemSP = RowItem.Pin();

	if (!RowItemSP.IsValid())
	{
		return SNullWidget::NullWidget;
	}

	using namespace UE::AdvancedRenamer::Private;

	TSharedRef<STextBlock> TextBlock = SNew(STextBlock)
		.ColorAndOpacity(FStyleColors::AccentWhite.GetSpecifiedColor())
		.Font(RegularFont)
		.Margin(FMargin(5.f, 0.f, 5.f, 0.f));
		
	if (ColumnName == FAdvancedRenamerPreviewListItem::OriginalNameColumnName)
	{
		TextBlock->SetText(FText::FromString(RowItemSP->OriginalName));
		RenamePanelSP->MinDesiredOriginalNameWidth = FMath::Max(RenamePanelSP->MinDesiredOriginalNameWidth, TextBlock->ComputeDesiredSize(1.f).X);
	}
	else if (ColumnName == FAdvancedRenamerPreviewListItem::NewNameColumnName)
	{
		TextBlock->SetText(FText::FromString(RowItemSP->NewName));
		RenamePanelSP->MinDesiredNewNameWidth = FMath::Max(RenamePanelSP->MinDesiredOriginalNameWidth, TextBlock->ComputeDesiredSize(1.f).X);
	}

	RenamePanelSP->UpdateRequiredListWidth();

	return TextBlock;
}

void SAdvancedRenamerPanel::Construct(const FArguments& InArgs)
{
	if (InArgs._SharedProvider.IsValid())
	{
		SharedProvider = InArgs._SharedProvider;
	}
	else
	{
		checkNoEntry();
	}

	int32 Count = Num();
	check(Count > 0);

	for (int32 Index = 0; Index < Count; ++Index)
	{
		if (!CanRename(Index))
		{
			RemoveIndex(Index);
			--Index;
			--Count;
			continue;
		}

		int32 Hash = GetHash(Index);
		FString OriginalName = GetOriginalName(Index);

		ListData.Add(MakeShared<FAdvancedRenamerPreviewListItem>(Hash, OriginalName));
	}

	CommandList = MakeShared<FUICommandList>();
	CommandList->MapAction(
		FGenericCommands::Get().Delete,
		FExecuteAction::CreateSP(this, &SAdvancedRenamerPanel::RemoveSelectedObjects),
		FCanExecuteAction()
	);

	using namespace UE::AdvancedRenamer::Private;

	// @formatter:off
	TSharedRef<SCanvas> Canvas = SNew(SCanvas)
		+ SCanvas::Slot()
		.Position(FVector2D(0.f, 0.f))
		.Size(WindowSize)
		[
			SNew(SColorBlock)
			.Color(FStyleColors::Background.GetSpecifiedColor())
		];
	// @formatter:on

	CreateLeftPane(Canvas);
	CreateRightPane(Canvas);

	// @formatter:off
	ChildSlot
	[
		Canvas
	];
	// @formatter:on

	if (FEngineAnalytics::IsAvailable())
	{
		FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.AdvancedRenamer.Opened"));
	}
}

void SAdvancedRenamerPanel::CreateLeftPane(TSharedRef<SCanvas> Canvas)
{	
	using namespace UE::AdvancedRenamer::Private;

	// @formatter:off
	Canvas->AddSlot()
		.Position(FVector2D(5.f, 5.f))
		.Size(FVector2D(LeftBlockWidth, 50.f))
		[
			CreateBaseName()
		];

	Canvas->AddSlot()
		.Position(FVector2D(5.f, 60.f))
		.Size(FVector2D(LeftBlockWidth, 102.f))
		[
			CreatePrefix()
		];

	Canvas->AddSlot()
		.Position(FVector2D(5.f, 167.f))
		.Size(FVector2D(LeftBlockWidth, 154.f))
		[
			CreateSuffix()
		];

	Canvas->AddSlot()
		.Position(FVector2D(5.f, 327.f))
		.Size(FVector2D(LeftBlockWidth, 168.f))
		[
			CreateSearchAndReplace()
		];
	// @formatter:on
}

TSharedRef<SWidget> SAdvancedRenamerPanel::CreateBaseName()
{
	using namespace UE::AdvancedRenamer::Private;

	// @formatter:off
	return SNew(SBorder)
		.BorderImage(FAppStyle::Get().GetBrush("DetailsView.CategoryTop"))
		.Content()
		[
			SNew(SCanvas)
			+ SCanvas::Slot()
			.Position(FVector2D(0.f, LabelOffsetY))
			.Size(TitleSize)
			[
				SNew(STextBlock)
				.ColorAndOpacity(FStyleColors::AccentBlue.GetSpecifiedColor())
				.Font(TitleFont)
				.Text(LOCTEXT("BaseNameTitle", "Name"))
			]
			+ SCanvas::Slot()
			.Position(FVector2D(0.f, TitleOffsetY) + CheckboxOffset)
			.Size(CheckboxSize)
			[
				SAssignNew(BaseNameCheckBox, SCheckBox)
				.IsChecked(this, &SAdvancedRenamerPanel::IsBaseNameChecked)
				.OnCheckStateChanged(this, &SAdvancedRenamerPanel::OnBaseNameCheckBoxChanged)
			]
			+ SCanvas::Slot()
			.Position(FVector2D(LabelStart, TitleOffsetY + LabelOffsetY))
			.Size(LabelSize)
			[
				SNew(STextBlock)
				.ColorAndOpacity(FStyleColors::AccentWhite.GetSpecifiedColor())
				.Font(RegularFont)
				.Text(LOCTEXT("BaseName", "Base Name"))
			]
			+ SCanvas::Slot()
			.Position(FVector2D(EntryStart, TitleOffsetY))
			.Size(EntrySize)
			[
				SAssignNew(BaseNameTextBox, SEditableTextBox)
				.BackgroundColor(FStyleColors::Background.GetSpecifiedColor())
				.ForegroundColor(FStyleColors::AccentWhite.GetSpecifiedColor())
				.Font(RegularFont)
				.HintText(LOCTEXT("BaseNameHint", "Base name"))
				.IsEnabled(false)
				.OnTextChanged(this, &SAdvancedRenamerPanel::OnBaseNameChanged)
			]
		];
	// @formatter:on
}

TSharedRef<SWidget> SAdvancedRenamerPanel::CreatePrefix()
{
	using namespace UE::AdvancedRenamer::Private;

	// @formatter:off
	return SNew(SBorder)
		.BorderImage(FAppStyle::Get().GetBrush("DetailsView.CategoryTop"))
		.Content()
		[
			SNew(SCanvas)

			// Title
			+ SCanvas::Slot()
			.Position(FVector2D(0.f, LabelOffsetY))
			.Size(TitleSize)
			[
				SNew(STextBlock)
				.ColorAndOpacity(FStyleColors::AccentBlue.GetSpecifiedColor())
				.Font(TitleFont)
				.Text(LOCTEXT("PrefixTitle", "Prefix"))
			]

			// Prefix name
			+ SCanvas::Slot()
			.Position(FVector2D(0.f, TitleOffsetY) + CheckboxOffset)
			.Size(CheckboxSize)
			[
				SAssignNew(PrefixCheckBox, SCheckBox)
				.IsChecked(this, &SAdvancedRenamerPanel::IsPrefixChecked)
				.OnCheckStateChanged(this, &SAdvancedRenamerPanel::OnPrefixCheckBoxChanged)
			]
			+ SCanvas::Slot()
			.Position(FVector2D(LabelStart, TitleOffsetY + LabelOffsetY))
			.Size(LabelSize)
			[
				SNew(STextBlock)
				.ColorAndOpacity(FStyleColors::AccentWhite.GetSpecifiedColor())
				.Font(RegularFont)
				.Text(LOCTEXT("Prefix", "Prefix"))
			]
			+ SCanvas::Slot()
			.Position(FVector2D(EntryStart, TitleOffsetY))
			.Size(EntrySize)
			[
				SAssignNew(PrefixTextBox, SEditableTextBox)
				.BackgroundColor(FStyleColors::Background.GetSpecifiedColor())
				.ForegroundColor(FStyleColors::AccentWhite.GetSpecifiedColor())
				.Font(RegularFont)
				.HintText(LOCTEXT("PrefixHint", "Prefix"))
				.IsEnabled(false)
				.OnTextChanged(this, &SAdvancedRenamerPanel::OnPrefixChanged)
			]

			// Remove prefix
			+ SCanvas::Slot()
			.Position(FVector2D(LabelStart, TitleOffsetY + LineHeight) + RadioOffset)
			.Size(CheckboxSize)
			[
				SAssignNew(PrefixRemoveCheckBox, SCheckBox)
				.Style(&FAdvancedRenamerStyle::Get().GetWidgetStyle<FCheckBoxStyle>("AdvancedRenamer.Style.BlackRadioButton"))
				.IsChecked(this, &SAdvancedRenamerPanel::IsPrefixRemoveChecked)
				.OnCheckStateChanged(this, &SAdvancedRenamerPanel::OnPrefixRemoveCheckBoxChanged)
				.IsEnabled(false)
			]
			+ SCanvas::Slot()
			.Position(FVector2D(LabelStart * 2.f, TitleOffsetY + LabelOffsetY + LineHeight))
			.Size(LabelSize)
			[
				SNew(STextBlock)
				.ColorAndOpacity(FStyleColors::AccentWhite.GetSpecifiedColor())
				.Font(RegularFont)
				.Text(LOCTEXT("PrefixRemove", "Remove Prefix"))
			]
			+ SCanvas::Slot()
			.Position(FVector2D(EntryStart, TitleOffsetY + LineHeight))
			.Size(SeparatorSize)
			[
				SAssignNew(PrefixSeparatorTextBox, SEditableTextBox)
				.BackgroundColor(FStyleColors::Background.GetSpecifiedColor())
				.ForegroundColor(FStyleColors::AccentWhite.GetSpecifiedColor())
				.Font(RegularFont)
				.IsEnabled(false)
				.Text(LOCTEXT("Underscore", "_"))
				.OnVerifyTextChanged(this, &SAdvancedRenamerPanel::OnPrefixSeparatorVerifyTextChanged)
				.OnTextChanged(this, &SAdvancedRenamerPanel::OnPrefixSeparatorChanged)
			]
			+ SCanvas::Slot()
			.Position(FVector2D(EntryStart + SeparatorSize.X + 5.f, TitleOffsetY + LabelOffsetY + LineHeight))
			.Size(LabelSize)
			[
				SNew(STextBlock)
				.ColorAndOpacity(FStyleColors::AccentWhite.GetSpecifiedColor())
				.Font(RegularFont)
				.Text(LOCTEXT("Separator", "(separator)"))
			]

			// Remove first
			+ SCanvas::Slot()
			.Position(FVector2D(LabelStart, TitleOffsetY + LineHeight * 2.f) + RadioOffset)
			.Size(CheckboxSize)
			[
				SAssignNew(PrefixRemoveCharactersCheckBox, SCheckBox)
				.Style(&FAdvancedRenamerStyle::Get().GetWidgetStyle<FCheckBoxStyle>("AdvancedRenamer.Style.BlackRadioButton"))
				.IsChecked(this, &SAdvancedRenamerPanel::IsPrefixRemoveCharactersChecked)
				.OnCheckStateChanged(this, &SAdvancedRenamerPanel::OnPrefixRemoveCharactersCheckBoxChanged)
				.IsEnabled(false)
			]
			+ SCanvas::Slot()
			.Position(FVector2D(LabelStart * 2.f, TitleOffsetY + LabelOffsetY + LineHeight * 2.f))
			.Size(LabelSize)
			[
				SNew(STextBlock)
				.ColorAndOpacity(FStyleColors::AccentWhite.GetSpecifiedColor())
				.Font(RegularFont)
				.Text(LOCTEXT("PrefixRemoveChars", "Remove First"))
			]
			+ SCanvas::Slot()
			.Position(FVector2D(EntryStart, TitleOffsetY + LineHeight * 2.f))
			.Size(SpinSize1)
			[

				SAssignNew(PrefixRemoveCharactersSpinBox, SSpinBox<uint8>)
				.Style(&FAppStyle::Get(), "Menu.SpinBox")
				.Font(RegularFont)
				.MinValue(1)
				.MaxValue(9)
				.Value(1)
				.IsEnabled(false)
				.OnValueChanged(this, &SAdvancedRenamerPanel::OnPrefixRemoveCharactersChanged)
			]
			+ SCanvas::Slot()
			.Position(FVector2D(EntryStart + SeparatorSize.X + 5.f, TitleOffsetY + LabelOffsetY + LineHeight * 2.f))
			.Size(LabelSize)
			[
				SNew(STextBlock)
				.ColorAndOpacity(FStyleColors::AccentWhite.GetSpecifiedColor())
				.Font(RegularFont)
				.Text(LOCTEXT("Characters", "character(s)"))
			]
		];
	// @formatter:on
}

TSharedRef<SWidget> SAdvancedRenamerPanel::CreateSuffix()
{
	using namespace UE::AdvancedRenamer::Private;

	// @formatter:off
	return SNew(SBorder)
		.BorderImage(FAppStyle::Get().GetBrush("DetailsView.CategoryTop"))
		.Content()
		[
			SNew(SCanvas)

			// Title
			+ SCanvas::Slot()
			.Position(FVector2D(0.f, LabelOffsetY))
			.Size(TitleSize)
			[
				SNew(STextBlock)
				.ColorAndOpacity(FStyleColors::AccentBlue.GetSpecifiedColor())
				.Font(TitleFont)
				.Text(LOCTEXT("SuffixTitle", "Suffix"))
			]

			// Suffix name
			+ SCanvas::Slot()
			.Position(FVector2D(0.f, TitleOffsetY) + CheckboxOffset)
			.Size(CheckboxSize)
			[
				SAssignNew(SuffixCheckBox, SCheckBox)
				.IsChecked(this, &SAdvancedRenamerPanel::IsSuffixChecked)
				.OnCheckStateChanged(this, &SAdvancedRenamerPanel::OnSuffixCheckBoxChanged)
			]
			+ SCanvas::Slot()
			.Position(FVector2D(LabelStart, TitleOffsetY + LabelOffsetY))
			.Size(LabelSize)
			[
				SNew(STextBlock)
				.ColorAndOpacity(FStyleColors::AccentWhite.GetSpecifiedColor())
				.Font(RegularFont)
				.Text(LOCTEXT("Suffix", "Suffix"))
			]
			+ SCanvas::Slot()
			.Position(FVector2D(EntryStart, TitleOffsetY))
			.Size(EntrySize)
			[
				SAssignNew(SuffixTextBox, SEditableTextBox)
				.BackgroundColor(FStyleColors::Background.GetSpecifiedColor())
				.ForegroundColor(FStyleColors::AccentWhite.GetSpecifiedColor())
				.Font(RegularFont)
				.HintText(LOCTEXT("SuffixHint", "Suffix"))
				.IsEnabled(false)
				.OnTextChanged(this, &SAdvancedRenamerPanel::OnSuffixChanged)
			]

			// Remove Suffix
			+ SCanvas::Slot()
			.Position(FVector2D(LabelStart, TitleOffsetY + LineHeight) + RadioOffset)
			.Size(CheckboxSize)
			[
				SAssignNew(SuffixRemoveCheckBox, SCheckBox)
				.Style(&FAdvancedRenamerStyle::Get().GetWidgetStyle<FCheckBoxStyle>("AdvancedRenamer.Style.BlackRadioButton"))
				.IsChecked(this, &SAdvancedRenamerPanel::IsSuffixRemoveChecked)
				.OnCheckStateChanged(this, &SAdvancedRenamerPanel::OnSuffixRemoveCheckBoxChanged)
				.IsEnabled(false)
			]
			+ SCanvas::Slot()
			.Position(FVector2D(LabelStart * 2.f, TitleOffsetY + LabelOffsetY + LineHeight))
			.Size(LabelSize)
			[
				SNew(STextBlock)
				.ColorAndOpacity(FStyleColors::AccentWhite.GetSpecifiedColor())
				.Font(RegularFont)
				.Text(LOCTEXT("SuffixRemove", "Remove Suffix"))
			]
			+ SCanvas::Slot()
			.Position(FVector2D(EntryStart, TitleOffsetY + LineHeight))
			.Size(SeparatorSize)
			[
				SAssignNew(SuffixSeparatorTextBox, SEditableTextBox)
				.BackgroundColor(FStyleColors::Background.GetSpecifiedColor())
				.ForegroundColor(FStyleColors::AccentWhite.GetSpecifiedColor())
				.Font(RegularFont)
				.IsEnabled(false)
				.Text(LOCTEXT("Underscore", "_"))
				.OnVerifyTextChanged(this, &SAdvancedRenamerPanel::OnSuffixSeparatorVerifyTextChanged)
				.OnTextChanged(this, &SAdvancedRenamerPanel::OnSuffixSeparatorChanged)
			]
			+ SCanvas::Slot()
			.Position(FVector2D(EntryStart + SeparatorSize.X + 5.f, TitleOffsetY + LabelOffsetY + LineHeight))
			.Size(LabelSize)
			[
				SNew(STextBlock)
				.ColorAndOpacity(FStyleColors::AccentWhite.GetSpecifiedColor())
				.Font(RegularFont)
				.Text(LOCTEXT("Separator", "(separator)"))
			]

			// Remove first
			+ SCanvas::Slot()
			.Position(FVector2D(LabelStart, TitleOffsetY + LineHeight * 2.f) + RadioOffset)
			.Size(CheckboxSize)
			[
				SAssignNew(SuffixRemoveCharactersCheckBox, SCheckBox)
				.Style(&FAdvancedRenamerStyle::Get().GetWidgetStyle<FCheckBoxStyle>("AdvancedRenamer.Style.BlackRadioButton"))
				.IsChecked(this, &SAdvancedRenamerPanel::IsSuffixRemoveCharactersChecked)
				.OnCheckStateChanged(this, &SAdvancedRenamerPanel::OnSuffixRemoveCharactersCheckBoxChanged)
				.IsEnabled(false)
			]
			+ SCanvas::Slot()
			.Position(FVector2D(LabelStart * 2.f, TitleOffsetY + LabelOffsetY + LineHeight * 2.f))
			.Size(LabelSize)
			[
				SNew(STextBlock)
				.ColorAndOpacity(FStyleColors::AccentWhite.GetSpecifiedColor())
				.Font(RegularFont)
				.Text(LOCTEXT("SuffixRemoveChars", "Remove Last"))
			]
			+ SCanvas::Slot()
			.Position(FVector2D(EntryStart, TitleOffsetY + LineHeight * 2.f))
			.Size(SpinSize1)
			[

				SAssignNew(SuffixRemoveCharactersSpinBox, SSpinBox<uint8>)
				.Style(&FAppStyle::Get(), "Menu.SpinBox")
				.Font(RegularFont)
				.MinValue(1)
				.MaxValue(9)
				.Value(1)
				.IsEnabled(false)
				.OnValueChanged(this, &SAdvancedRenamerPanel::OnSuffixRemoveCharactersChanged)
			]
			+ SCanvas::Slot()
			.Position(FVector2D(EntryStart + SeparatorSize.X + 5.f, TitleOffsetY + LabelOffsetY + LineHeight * 2.f))
			.Size(LabelSize)
			[
				SNew(STextBlock)
				.ColorAndOpacity(FStyleColors::AccentWhite.GetSpecifiedColor())
				.Font(RegularFont)
				.Text(LOCTEXT("Characters", "character(s)"))
			]

			// Remove Number
			+ SCanvas::Slot()
			.Position(FVector2D(LabelStart, TitleOffsetY + LineHeight * 3.f) + CheckboxOffset)
			.Size(CheckboxSize)
			[
				SAssignNew(SuffixRemoveNumberCheckBox, SCheckBox)
				.IsChecked(this, &SAdvancedRenamerPanel::IsSuffixRemoveNumberChecked)
				.OnCheckStateChanged(this, &SAdvancedRenamerPanel::OnSuffixRemoveNumberCheckBoxChanged)
				.IsEnabled(false)
			]
			+ SCanvas::Slot()
			.Position(FVector2D(LabelStart * 2.f, TitleOffsetY + LabelOffsetY + LineHeight * 3.f))
			.Size(LabelSize)
			[
				SNew(STextBlock)
				.ColorAndOpacity(FStyleColors::AccentWhite.GetSpecifiedColor())
				.Font(RegularFont)
				.Text(LOCTEXT("SuffixRemoveNumber", "Remove Number"))
			]

			// Number
			+ SCanvas::Slot()
			.Position(FVector2D(LabelStart, TitleOffsetY + LineHeight * 4.f) + CheckboxOffset)
			.Size(CheckboxSize)
			[
				SAssignNew(SuffixNumberCheckBox, SCheckBox)
				.IsChecked(this, &SAdvancedRenamerPanel::IsSuffixNumberChecked)
				.OnCheckStateChanged(this, &SAdvancedRenamerPanel::OnSuffixNumberCheckBoxChanged)
				.IsEnabled(false)
			]
			+ SCanvas::Slot()
			.Position(FVector2D(LabelStart * 2.f, TitleOffsetY + LabelOffsetY + LineHeight * 4.f))
			.Size(LabelSize)
			[
				SNew(STextBlock)
				.ColorAndOpacity(FStyleColors::AccentWhite.GetSpecifiedColor())
				.Font(RegularFont)
				.Text(LOCTEXT("SuffixNumber", "Number"))
			]
			+ SCanvas::Slot()
			.Position(FVector2D(EntryStart , TitleOffsetY + LabelOffsetY + LineHeight * 4.f))
			.Size(LabelSize)
			[
				SNew(STextBlock)
				.ColorAndOpacity(FStyleColors::AccentWhite.GetSpecifiedColor())
				.Font(RegularFont)
				.Text(LOCTEXT("Start", "Start"))
			]
			+ SCanvas::Slot()
			.Position(FVector2D(EntryStart + 33.f, TitleOffsetY + LineHeight * 4.f))
			.Size(SpinSize3)
			[

				SAssignNew(SuffixNumberStartSpinBox, SSpinBox<int32>)
				.Style(&FAppStyle::Get(), "Menu.SpinBox")
				.Font(RegularFont)
				.MinValue(0)
				.MaxValue(999)
				.Value(1)
				.IsEnabled(false)
				.OnValueChanged(this, &SAdvancedRenamerPanel::OnSuffixNumberStartChanged)
			]
			+ SCanvas::Slot()
			.Position(FVector2D(EntryStart + 33.f + SpinSize3.X + 5.f, TitleOffsetY + LabelOffsetY + LineHeight * 4.f))
			.Size(LabelSize)
			[
				SNew(STextBlock)
				.ColorAndOpacity(FStyleColors::AccentWhite.GetSpecifiedColor())
				.Font(RegularFont)
				.Text(LOCTEXT("Step", "Step"))
			]
			+ SCanvas::Slot()
			.Position(FVector2D(EntryStart + 33.f + SpinSize3.X + 36.f, TitleOffsetY + LineHeight * 4.f))
			.Size(SpinSize2)
			[

				SAssignNew(SuffixNumberStepSpinBox, SSpinBox<int32>)
				.Style(&FAppStyle::Get(), "Menu.SpinBox")
				.Font(RegularFont)
				.MinValue(1)
				.MaxValue(99)
				.Value(1)
				.IsEnabled(false)
				.OnValueChanged(this, &SAdvancedRenamerPanel::OnSuffixNumberStepChanged)
			]
		];
	// @formatter:on
}

TSharedRef<SWidget> SAdvancedRenamerPanel::CreateSearchAndReplace()
{
	using namespace UE::AdvancedRenamer::Private;

	// @formatter:off
	return SNew(SBorder)
		.BorderImage(FAppStyle::Get().GetBrush("DetailsView.CategoryTop"))
		.Content()
		[
			SNew(SCanvas)

			// Title
			+ SCanvas::Slot()
			.Position(FVector2D(0.f, LabelOffsetY))
			.Size(TitleSize)
			[
				SNew(STextBlock)
				.ColorAndOpacity(FStyleColors::AccentBlue.GetSpecifiedColor())
				.Font(TitleFont)
				.Text(LOCTEXT("SearchReplaceTitle", "Search and Replace"))
			]

			+ SCanvas::Slot()
			.Position(FVector2D(0.f, TitleOffsetY) + RadioOffset)
			.Size(CheckboxSize)
			[
				SAssignNew(SearchReplacePlainTextCheckbox, SCheckBox)
				.Style(&FAdvancedRenamerStyle::Get().GetWidgetStyle<FCheckBoxStyle>("AdvancedRenamer.Style.BlackRadioButton"))
				.IsChecked(this, &SAdvancedRenamerPanel::IsSearchReplacePlainTextChecked)
				.OnCheckStateChanged(this, &SAdvancedRenamerPanel::OnSearchReplacePlainTextCheckBoxChanged)
			]

			+ SCanvas::Slot()
			.Position(FVector2D(0.f + LabelStart, TitleOffsetY + LabelOffsetY))
			.Size(LabelSize)
			[
				SNew(STextBlock)
				.ColorAndOpacity(FStyleColors::AccentWhite.GetSpecifiedColor())
				.Font(RegularFont)
				.Text(LOCTEXT("PlainText", "Plain Text"))
			]

			+ SCanvas::Slot()
			.Position(FVector2D(92.f, TitleOffsetY) + RadioOffset)
			.Size(CheckboxSize)
			[
				SAssignNew(SearchReplaceRegexCheckbox, SCheckBox)
				.Style(&FAdvancedRenamerStyle::Get().GetWidgetStyle<FCheckBoxStyle>("AdvancedRenamer.Style.BlackRadioButton"))
				.IsChecked(this, &SAdvancedRenamerPanel::IsSearchReplaceRegexChecked)
				.OnCheckStateChanged(this, &SAdvancedRenamerPanel::OnSearchReplaceRegexCheckBoxChanged)
			]

			+ SCanvas::Slot()
			.Position(FVector2D(92.f + LabelStart, TitleOffsetY + LabelOffsetY))
			.Size(LabelSize)
			[
				SNew(STextBlock)
				.ColorAndOpacity(FStyleColors::AccentWhite.GetSpecifiedColor())
				.Font(RegularFont)
				.Text(LOCTEXT("Regex", "Regex"))
			]

			+ SCanvas::Slot()
			.Position(FVector2D(200.f, TitleOffsetY) + CheckboxOffset)
			.Size(CheckboxSize)
			[
				SAssignNew(SearchReplaceIgnoreCaseCheckBox, SCheckBox)
				.IsChecked(this, &SAdvancedRenamerPanel::IsSearchReplaceIgnoreCaseChecked)
				.OnCheckStateChanged(this, &SAdvancedRenamerPanel::OnSearchReplaceIgnoreCaseCheckBoxChanged)
			]

			+ SCanvas::Slot()
			.Position(FVector2D(200.f + LabelStart, TitleOffsetY + LabelOffsetY))
			.Size(LabelSize)
			[
				SNew(STextBlock)
				.ColorAndOpacity(FStyleColors::AccentWhite.GetSpecifiedColor())
				.Font(RegularFont)
				.Text(LOCTEXT("IgnoreCase", "Ignore Case"))
			]

			+ SCanvas::Slot()
			.Position(FVector2D(0.f, TitleOffsetY + LineHeight))
			.Size(FVector2D(LeftBlockWidth - 4.f, 54.f))
			[
				SAssignNew(SearchReplaceSearchTextBox, SMultiLineEditableTextBox)
				.BackgroundColor(FStyleColors::Background.GetSpecifiedColor())
				.ForegroundColor(FStyleColors::AccentWhite.GetSpecifiedColor())
				.Font(RegularFont)
				.AllowMultiLine(false)
				.AutoWrapText(true)
				.HintText(LOCTEXT("RegeSearchHint", "Search"))
				.WrappingPolicy(ETextWrappingPolicy::AllowPerCharacterWrapping)
				.IsEnabled(false)
				.OnTextChanged(this, &SAdvancedRenamerPanel::OnSearchReplaceSearchTextChanged)
			]

			+ SCanvas::Slot()
			.Position(FVector2D(0.f, TitleOffsetY + LineHeight + 59.f))
			.Size(FVector2D(LeftBlockWidth - 4.f, 54.f))
			[
				SAssignNew(SearchReplaceReplaceTextBox, SMultiLineEditableTextBox)
				.BackgroundColor(FStyleColors::Background.GetSpecifiedColor())
				.ForegroundColor(FStyleColors::AccentWhite.GetSpecifiedColor())
				.Font(RegularFont)
				.AllowMultiLine(false)
				.AutoWrapText(true)
				.HintText(LOCTEXT("RegeReplaceHint", "Replace"))
				.WrappingPolicy(ETextWrappingPolicy::AllowPerCharacterWrapping)
				.IsEnabled(false)
				.OnTextChanged(this, &SAdvancedRenamerPanel::OnSearchReplaceReplaceTextChanged)
			]
		];
	// @formatter:on
}

void SAdvancedRenamerPanel::CreateRightPane(TSharedRef<SCanvas> Canvas)
{
	using namespace UE::AdvancedRenamer::Private;

	// @formatter:off
	Canvas->AddSlot()
		.Position(FVector2D(RightBlockOffsetX, 5.f))
		.Size(RightBlockSize)
		[
			CreateRenamePreview()
		];
	// @formatter:on
}

TSharedRef<SWidget> SAdvancedRenamerPanel::CreateRenamePreview()
{
	using namespace UE::AdvancedRenamer::Private;

	// @formatter:off
	return SNew(SBorder)
		.BorderImage(FAppStyle::Get().GetBrush("DetailsView.CategoryTop"))
		.Content()
		[
			SNew(SCanvas)
				
			// Title
			+ SCanvas::Slot()
			.Position(FVector2D(0.f, LabelOffsetY))
			.Size(TitleSize)
			[
				SNew(STextBlock)
				.ColorAndOpacity(FStyleColors::AccentBlue.GetSpecifiedColor())
				.Font(TitleFont)
				.Text(LOCTEXT("ObjectRenamePreviewTitle", "Rename Preview"))
			]

			// List View
			+ SCanvas::Slot()
			.Position(FVector2D(0.f, TitleOffsetY))
			.Size(ListViewSize)
			[
				SNew(SScrollBox)
				.Orientation(EOrientation::Orient_Horizontal)
				+ SScrollBox::Slot()
				[
					SAssignNew(RenamePreviewListBox, SBox)
					.WidthOverride(ListViewSize.Y)
					.HeightOverride(ListViewSize.X)
					.HAlign(EHorizontalAlignment::HAlign_Fill)
					.VAlign(EVerticalAlignment::VAlign_Fill)
					.Content()
					[
						SAssignNew(RenamePreviewList, SListView<FObjectRenamePreviewListItemPtr>)
						.ItemHeight(ListLineHeight)
						.ListItemsSource(&ListData)
						.OnGenerateRow(this, &SAdvancedRenamerPanel::OnGenerateRowForList)
						.HeaderRow(
							SAssignNew(RenamePreviewListHeaderRow, SHeaderRow)
							+ SHeaderRow::Column(FAdvancedRenamerPreviewListItem::OriginalNameColumnName)
							.DefaultLabel(LOCTEXT("From", "From"))
							.FillWidth(ListViewSize.X / 2.f)
							+ SHeaderRow::Column(FAdvancedRenamerPreviewListItem::NewNameColumnName)
							.DefaultLabel(LOCTEXT("To", "To"))
							.FillWidth(ListViewSize.X / 2.f)
						)
						.OnKeyDownHandler(this, &SAdvancedRenamerPanel::OnListViewKeyDown)
						.OnContextMenuOpening(this, &SAdvancedRenamerPanel::GenerateListViewContextMenu)
					]
				]
			]

			// Apply Button
			+ SCanvas::Slot()
			.Position(FVector2D(0.f, TitleOffsetY + ListViewSize.Y + 5.f))
			.Size(FVector2D(ListViewSize.X, ApplyButtonHeight))
			[
				SAssignNew(ApplyButton, SButton)
				.ButtonStyle(FAdvancedRenamerStyle::Get(), "AdvancedRenamer.Style.DarkButton")
				.ToolTipText(LOCTEXT("ApplyRenameToolTip", "Renames all actors."))
				.HAlign(EHorizontalAlignment::HAlign_Center)
				.VAlign(EVerticalAlignment::VAlign_Center)
				.IsEnabled(false)
				.OnClicked(this, &SAdvancedRenamerPanel::OnApplyButtonClicked)
				[
					SNew(STextBlock)
					.ColorAndOpacity(FStyleColors::AccentWhite.GetSpecifiedColor())
					.Font(RegularFont)
					.Text(LOCTEXT("ApplyRename", "Apply"))
				]
			]
		];
	// @formatter:on
}

bool SAdvancedRenamerPanel::RenameObjects()
{
	if (!bValidNames)
	{
		return false;
	}

	FScopedTransaction Transaction(LOCTEXT("AdvancedObjectRename", "Advanced Object Rename"));
	int32 ItemsRenamed = 0;
	int32 ItemCount = Num();
	
	for (int32 Index = 0; Index < ItemCount; ++Index)
	{
		if (!ListData[Index].IsValid())
		{
			continue;
		}

		if (!IsValidIndex(Index))
		{
			continue;
		}

		if (ListData[Index]->NewName.Len() == 0)
		{
			continue;
		}

		if (ExecuteRename(Index, ListData[Index]->NewName))
		{
			++ItemsRenamed;
		}
	}

	//Cancel Transaction if no Items could successfully Rename
	if (ItemsRenamed == 0)
	{
		Transaction.Cancel();
		//TODO: Should we keep Window Open as no renames occurred. Might be something the User did not expect
		//return false;
	}
	
	return true;
}

bool SAdvancedRenamerPanel::CloseWindow()
{
	TSharedPtr<SWindow> CurrentWindow = FSlateApplication::Get().FindWidgetWindow(SharedThis(this));

	if (CurrentWindow.IsValid())
	{
		CurrentWindow->RequestDestroyWindow();
		return true;
	}

	return false;
}

FString SAdvancedRenamerPanel::CreateNewName(int32 Index) const
{
	if (!IsValidIndex(Index))
	{
		return "";
	}

	FString DisplayName = GetOriginalName(Index);

	if (DisplayName.Len() == 0)
	{
		return DisplayName;
	}

	return ApplyRename(DisplayName, Index);
}

FString SAdvancedRenamerPanel::ApplyRename(const FString& OriginalName, int32 Index) const
{
	FString NewName = OriginalName;

	if (bBaseName)
	{
		NewName = ApplyBaseName(NewName);
	}

	if (bPrefix)
	{
		NewName = ApplyPrefix(NewName);
	}

	if (bSuffix)
	{
		NewName = ApplySuffix(NewName, Index);
	}

	if (bSearchReplacePlainText)
	{
		NewName = ApplySearchPlainText(NewName);
	}
	else if (bSearchReplaceRegex)
	{
		NewName = ApplySearchReplaceRegex(NewName);
	}

	return NewName;
}

FString SAdvancedRenamerPanel::ApplyBaseName(const FString& OriginalName) const
{
	if (!BaseNameTextBox.IsValid())
	{
		return OriginalName;
	}

	FString BaseName = BaseNameTextBox->GetText().ToString();
	
	if (BaseName.Len() == 0)
	{
		return OriginalName;
	}

	return BaseName;
}

FString SAdvancedRenamerPanel::ApplyPrefix(const FString& OriginalName) const
{
	FString Output = OriginalName;

	if (bPrefixRemove)
	{
		if (PrefixSeparatorTextBox.IsValid())
		{
			FString Separator = PrefixSeparatorTextBox->GetText().ToString();

			if (Separator.Len() != 0)
			{
				int32 PrefixStart = Output.Find(Separator, ESearchCase::IgnoreCase);

				if (PrefixStart >= 0)
				{
					Output = Output.Mid(PrefixStart + Separator.Len());
				}
			}
		}
	}
	else if (bPrefixRemoveCharacters)
	{
		if (PrefixRemoveCharactersSpinBox.IsValid())
		{
			Output = Output.RightChop(PrefixRemoveCharactersSpinBox->GetValue());
		}
	}

	if (PrefixTextBox.IsValid())
	{
		FString Prefix = PrefixTextBox->GetText().ToString();

		if (Prefix.Len() > 0)
		{
			Output = Prefix + Output;
		}
	}

	return Output;
}

FString SAdvancedRenamerPanel::ApplySuffix(const FString& OriginalName, int32 Index) const
{
	static const TCHAR FirstDigit = '0';
	static const TCHAR LastDigit = '9';

	FString Output = OriginalName;

	if (bSuffixRemove)
	{
		if (SuffixSeparatorTextBox.IsValid())
		{
			FString Separator = SuffixSeparatorTextBox->GetText().ToString();

			if (Separator.Len() != 0)
			{
				int32 SuffixStart = Output.Find(Separator, ESearchCase::IgnoreCase, ESearchDir::FromEnd);

				if (SuffixStart >= 0)
				{
					Output = Output.Mid(0, SuffixStart);
				}
			}
		}
	}
	else if (bSuffixRemoveCharacters)
	{
		if (SuffixRemoveCharactersSpinBox.IsValid())
		{
			Output = Output.LeftChop(SuffixRemoveCharactersSpinBox->GetValue());
		}
	}

	if (bSuffixRemoveNumber)
	{
		int32 LastDigitIndex = Output.Len() - 1;

		while(LastDigitIndex >= 0)
		{
			if (Output[LastDigitIndex] < FirstDigit || Output[LastDigitIndex] > LastDigit)
			{
				break;
			}

			--LastDigitIndex;
		}

		Output = Output.Mid(0, LastDigitIndex + 1);
	}

	if (SuffixTextBox.IsValid())
	{
		FString Suffix = SuffixTextBox->GetText().ToString();

		if (Suffix.Len() > 0)
		{
			Output = Output + Suffix;
		}
	}

	if (bSuffixNumber)
	{
		if (SuffixNumberStartSpinBox.IsValid() && SuffixNumberStepSpinBox.IsValid())
		{
			int32 Start = SuffixNumberStartSpinBox->GetValue();
			int32 Step = SuffixNumberStepSpinBox->GetValue();
			int32 Current = Start + (Index * Step);

			Output += FString::FromInt(Current);
		}
	}

	return Output;
}

FString SAdvancedRenamerPanel::ApplySearchPlainText(const FString& OriginalName) const
{
	if (!SearchReplaceSearchTextBox.IsValid() || !SearchReplaceReplaceTextBox.IsValid())
	{
		return OriginalName;
	}

	FString Search = SearchReplaceSearchTextBox->GetPlainText().ToString();

	if (Search.Len() == 0)
	{
		return OriginalName;
	}

	FString Replace = SearchReplaceReplaceTextBox->GetPlainText().ToString();

	if (Replace.Len() == 0)
	{
		UE_LOG(LogARP, Warning, TEXT("Regex: Empty replacement string."));
	}

	return OriginalName.Replace(*Search, *Replace, bSearchReplaceIgnoreCase ? ESearchCase::IgnoreCase : ESearchCase::CaseSensitive);
}

FString SAdvancedRenamerPanel::ApplySearchReplaceRegex(const FString& OriginalName) const
{
	if (!SearchReplaceSearchTextBox.IsValid() || !SearchReplaceReplaceTextBox.IsValid())
	{
		return OriginalName;
	}

	FString Pattern = SearchReplaceSearchTextBox->GetPlainText().ToString();

	if (Pattern.Len() == 0)
	{
		return OriginalName;
	}
	
	FRegexPattern RegexPattern = FRegexPattern(
		Pattern, 
		bSearchReplaceIgnoreCase ? ERegexPatternFlags::CaseInsensitive : ERegexPatternFlags::None
	);

	FString ReplaceString = SearchReplaceReplaceTextBox->GetPlainText().ToString();

	if (ReplaceString.Len() == 0)
	{
		UE_LOG(LogARP, Warning, TEXT("Regex: Empty replacement string."));
	}

	return RegexReplace(OriginalName, RegexPattern, ReplaceString);
}

FString SAdvancedRenamerPanel::RegexReplace(const FString& OriginalString, const FRegexPattern Pattern, const FString& ReplaceString) const
{
	static const FString EscapeString = "\\";
	static const TCHAR EscapeChar = EscapeString[0];
	static const FString GroupString = "$";
	static const TCHAR GroupChar = GroupString[0];
	static const TCHAR FirstDigit = '0';
	static const TCHAR LastDigit = '9';
	static const TCHAR NullChar = 0;

	FRegexMatcher Matcher(Pattern, OriginalString);

	FString Output = "";
	int32 StartCharIdx = 0;
	bool bReadingGroupName = false;
	int32 GroupIndex = INDEX_NONE;
	
	while (Matcher.FindNext())
	{
		// Add on part of string after start/previous match
		if (StartCharIdx != Matcher.GetMatchBeginning())
		{
			Output += OriginalString.Mid(StartCharIdx, Matcher.GetMatchBeginning() - StartCharIdx);
		}

		bool bEscaped = false;

		for (int32 CharIdx = 0; CharIdx <= ReplaceString.Len(); ++CharIdx)
		{
			const TCHAR& Char = CharIdx < ReplaceString.Len() ? ReplaceString[CharIdx] : NullChar;

			if (bReadingGroupName)
			{
				// Build group index
				if (Char >= FirstDigit && Char <= LastDigit)
				{
					if (GroupIndex == INDEX_NONE)
					{
						GroupIndex = 0;
					}
					else if (GroupIndex > 0)
					{
						GroupIndex *= 10;
					}

					int32 NextDigit = static_cast<int32>(Char - FirstDigit);					
					GroupIndex += NextDigit;
					continue;
				}
				// We've read a group index, add it to the output string
				else if (GroupIndex > 0)
				{
					if (Matcher.GetCaptureGroupBeginning(GroupIndex) == INDEX_NONE)
					{
						UE_LOG(LogARP, Error, TEXT("Regex: Capture group does not exist %d."), GroupIndex);
					}

					Output += Matcher.GetCaptureGroup(GroupIndex);
				}
				// $0 matches the entire matched string
				else if (GroupIndex == 0)
				{
					Output += OriginalString.Mid(Matcher.GetMatchBeginning(), Matcher.GetMatchEnding() - Matcher.GetMatchBeginning());
				}
				// An unescaped $
				else
				{
					UE_LOG(LogARP, Error, TEXT("Regex: Unescaped %s."), *GroupString);

					Output += GroupString;
				}

				bReadingGroupName = false;
				// Continue regular parsing of this character.
			}

			// Check for special chars
			if (!bEscaped)
			{
				if (Char == EscapeChar)
				{
					bEscaped = true;
					continue;
				}

				if (Char == GroupChar)
				{
					bReadingGroupName = true;
					GroupIndex = INDEX_NONE;
					continue;
				}
			}
			else
			{
				// If the last char is a \ assume that it's not an escape char
				if (Char == NullChar)
				{
					UE_LOG(LogARP, Error, TEXT("Regex: Unescaped %s."), *EscapeString);

					Output += EscapeChar;
				}
			}

			if (Char != NullChar)
			{
				Output += Char;
			}

			bEscaped = false;
		}

		StartCharIdx = Matcher.GetMatchEnding();
	}

	// Add on the end of the string after the last match
	if (StartCharIdx < OriginalString.Len())
	{
		Output += OriginalString.Mid(StartCharIdx);
	}

	return Output;
}

void SAdvancedRenamerPanel::UpdateEnables()
{
	if (BaseNameTextBox.IsValid())
	{
		BaseNameTextBox->SetEnabled(bBaseName);
	}

	if (PrefixTextBox.IsValid())
	{
		PrefixTextBox->SetEnabled(bPrefix);
	}

	if (PrefixRemoveCheckBox.IsValid())
	{
		PrefixRemoveCheckBox->SetEnabled(bPrefix);
	}

	if (PrefixSeparatorTextBox.IsValid())
	{
		PrefixSeparatorTextBox->SetEnabled(bPrefix && bPrefixRemove);
	}

	if (PrefixRemoveCharactersCheckBox.IsValid())
	{
		PrefixRemoveCharactersCheckBox->SetEnabled(bPrefix);
	}

	if (PrefixRemoveCharactersSpinBox.IsValid())
	{
		PrefixRemoveCharactersSpinBox->SetEnabled(bPrefix && bPrefixRemoveCharacters);
	}

	if (SuffixTextBox.IsValid())
	{
		SuffixTextBox->SetEnabled(bSuffix);
	}

	if (SuffixRemoveCheckBox.IsValid())
	{
		SuffixRemoveCheckBox->SetEnabled(bSuffix);
	}

	if (SuffixSeparatorTextBox.IsValid())
	{
		SuffixSeparatorTextBox->SetEnabled(bSuffix && bSuffixRemove);
	}

	if (SuffixRemoveCharactersCheckBox.IsValid())
	{
		SuffixRemoveCharactersCheckBox->SetEnabled(bSuffix);
	}

	if (SuffixRemoveCharactersSpinBox.IsValid())
	{
		SuffixRemoveCharactersSpinBox->SetEnabled(bSuffix && bSuffixRemoveCharacters);
	}

	if (SuffixRemoveNumberCheckBox.IsValid())
	{
		SuffixRemoveNumberCheckBox->SetEnabled(bSuffix && !bSuffixNumber);
	}

	if (SuffixNumberCheckBox.IsValid())
	{
		SuffixNumberCheckBox->SetEnabled(bSuffix);
	}

	if (SuffixNumberStartSpinBox.IsValid())
	{
		SuffixNumberStartSpinBox->SetEnabled(bSuffix && bSuffixNumber);
	}

	if (SuffixNumberStepSpinBox.IsValid())
	{
		SuffixNumberStepSpinBox->SetEnabled(bSuffix && bSuffixNumber);
	}

	if (SearchReplaceIgnoreCaseCheckBox.IsValid())
	{
		SearchReplaceIgnoreCaseCheckBox->SetEnabled(bSearchReplacePlainText || bSearchReplaceRegex);
	}

	if (SearchReplaceSearchTextBox.IsValid())
	{
		SearchReplaceSearchTextBox->SetEnabled(bSearchReplacePlainText || bSearchReplaceRegex);
	}

	if (SearchReplaceReplaceTextBox.IsValid())
	{
		SearchReplaceReplaceTextBox->SetEnabled(bSearchReplacePlainText || bSearchReplaceRegex);
	}

	if (ApplyButton.IsValid())
	{
		ApplyButton->SetEnabled(bValidNames);
	}

	RequestListViewRefresh();
}

void SAdvancedRenamerPanel::RequestListViewRefresh()
{
	bListNeedsUpdate = true;
}

void SAdvancedRenamerPanel::RefreshListView(const double InCurrentTime)
{
	bool bRemovedObjects = false;
	bValidNames = false;

	for (int32 Index = 0; Index < ListData.Num(); ++Index)
	{
		if (!ListData[Index].IsValid() || !IsValidIndex(Index))
		{
			RemoveIndex(Index);
			--Index;
			bRemovedObjects = true;
			continue;
		}

		// Force recreation
		ListData[Index]->NewName = CreateNewName(Index);

		if (ListData[Index]->NewName.Len() == 0)
		{
			continue;
		}

		if (GetOriginalName(Index) == ListData[Index]->NewName)
		{
			continue;
		}

		bValidNames = true;
	}

	if (bRemovedObjects)
	{
		RenamePreviewList->RequestListRefresh();
	}

	UpdateEnables();

	MinDesiredOriginalNameWidth = 0.f;
	MinDesiredNewNameWidth = 0.f;
	RenamePreviewList->RebuildList();

	ListLastUpdateTime = InCurrentTime;
	bListNeedsUpdate = false;
}

void SAdvancedRenamerPanel::UpdateRequiredListWidth()
{
	if (!RenamePreviewListBox.IsValid() || !RenamePreviewListHeaderRow.IsValid())
	{
		return;
	}

	if (MinDesiredNewNameWidth == 0.f)
	{
		return;
	}

	static const float ListViewWidth = 277.f;
	float ActualWidth = FMath::Max(ListViewWidth, MinDesiredOriginalNameWidth + MinDesiredNewNameWidth + 20.f);

	RenamePreviewListBox->SetWidthOverride(ActualWidth);
	
	RenamePreviewListHeaderRow->SetColumnWidth(
		FAdvancedRenamerPreviewListItem::OriginalNameColumnName, 
		(MinDesiredOriginalNameWidth + 10.f) / (MinDesiredOriginalNameWidth + MinDesiredNewNameWidth + 20.f) * ActualWidth
	);

	RenamePreviewListHeaderRow->SetColumnWidth(
		FAdvancedRenamerPreviewListItem::NewNameColumnName, 
		(MinDesiredNewNameWidth + 10.f) / (MinDesiredOriginalNameWidth + MinDesiredNewNameWidth + 20.f) * ActualWidth
	);

	RenamePreviewListHeaderRow->RefreshColumns();
}

void SAdvancedRenamerPanel::RemoveSelectedObjects()
{
	if (!RenamePreviewList.IsValid() || RenamePreviewList->GetNumItemsSelected() == 0)
	{
		return;
	}

	TArray<FObjectRenamePreviewListItemPtr> SelectedItems = RenamePreviewList->GetSelectedItems();

	if (SelectedItems.Num() == 0)
	{
		return;
	}

	bool bMadeChange = false;

	for (int32 Index = 0; Index < ListData.Num(); ++Index)
	{
		if (!ListData[Index].IsValid())
		{
			RemoveIndex(Index);
			bMadeChange = true;
			--Index;
			continue;
		}

		int32 MatchIdx = INDEX_NONE;

		for (int32 SelectedIdx = 0; SelectedIdx < SelectedItems.Num(); ++SelectedIdx)
		{
			if (!SelectedItems[SelectedIdx].IsValid())
			{
				continue;
			}

			if (SelectedItems[SelectedIdx]->Hash != ListData[Index]->Hash)
			{
				continue;
			}

			MatchIdx = SelectedIdx;
			break;
		}

		if (MatchIdx == INDEX_NONE)
		{
			continue;
		}

		RemoveIndex(Index);
		--Index;
		SelectedItems.RemoveAt(MatchIdx);
		bMadeChange = true;

		if (SelectedItems.Num() == 0)
		{
			break;
		}
	}

	if (bMadeChange)
	{
		if (ListData.Num() == 0)
		{
			if (CloseWindow())
			{
				return;
			}
		}

		RenamePreviewList->RequestListRefresh();
	}
}

void SAdvancedRenamerPanel::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	if (ListLastUpdateTime == 0)
	{
		ListLastUpdateTime = InCurrentTime;
	}
	else if (bListNeedsUpdate && InCurrentTime >= (ListLastUpdateTime + MinUpdateFrequency))
	{
		RefreshListView(InCurrentTime);
	}
}

void SAdvancedRenamerPanel::OnBaseNameCheckBoxChanged(ECheckBoxState NewState)
{
	bBaseName = NewState == ECheckBoxState::Checked;

	UpdateEnables();
}

void SAdvancedRenamerPanel::OnBaseNameChanged(const FText& NewText)
{
	RequestListViewRefresh();
}

void SAdvancedRenamerPanel::OnPrefixCheckBoxChanged(ECheckBoxState NewState)
{
	bPrefix = NewState == ECheckBoxState::Checked;

	UpdateEnables();
}

void SAdvancedRenamerPanel::OnPrefixChanged(const FText& NewText)
{
	RequestListViewRefresh();
}

void SAdvancedRenamerPanel::OnPrefixRemoveCheckBoxChanged(ECheckBoxState NewState)
{
	bPrefixRemove = NewState == ECheckBoxState::Checked;

	if (bPrefixRemove)
	{
		bPrefixRemoveCharacters = false;
	}

	UpdateEnables();
}

bool SAdvancedRenamerPanel::OnPrefixSeparatorVerifyTextChanged(const FText& InText, FText& OutErrorText) const
{
	if (InText.ToString().Len() > 1)
	{
		OutErrorText = LOCTEXT("SeparatorError", "Separators can only be a single character.");
		return false;
	}

	return true;
}

void SAdvancedRenamerPanel::OnPrefixSeparatorChanged(const FText& NewText)
{
	RequestListViewRefresh();
}

void SAdvancedRenamerPanel::OnPrefixRemoveCharactersCheckBoxChanged(ECheckBoxState NewState)
{
	bPrefixRemoveCharacters = NewState == ECheckBoxState::Checked;

	if (bPrefixRemoveCharacters)
	{
		bPrefixRemove = false;
	}

	UpdateEnables();
}

void SAdvancedRenamerPanel::OnPrefixRemoveCharactersChanged(uint8 NewValue)
{
	RequestListViewRefresh();
}

void SAdvancedRenamerPanel::OnSuffixCheckBoxChanged(ECheckBoxState NewState)
{
	bSuffix = NewState == ECheckBoxState::Checked;

	UpdateEnables();
}

void SAdvancedRenamerPanel::OnSuffixChanged(const FText& NewText)
{
	RequestListViewRefresh();
}

void SAdvancedRenamerPanel::OnSuffixRemoveCheckBoxChanged(ECheckBoxState NewState)
{
	bSuffixRemove = NewState == ECheckBoxState::Checked;

	if (bSuffixRemove)
	{
		bSuffixRemoveCharacters = false;
	}

	UpdateEnables();
}

bool SAdvancedRenamerPanel::OnSuffixSeparatorVerifyTextChanged(const FText& InText, FText& OutErrorText) const
{
	if (InText.ToString().Len() > 1)
	{
		OutErrorText = LOCTEXT("SeparatorError", "Separators can only be a single character.");
		return false;
	}

	return true;
}

void SAdvancedRenamerPanel::OnSuffixSeparatorChanged(const FText& NewText)
{
	RequestListViewRefresh();
}

void SAdvancedRenamerPanel::OnSuffixRemoveCharactersCheckBoxChanged(ECheckBoxState NewState)
{
	bSuffixRemoveCharacters = NewState == ECheckBoxState::Checked;

	if (bSuffixRemoveCharacters)
	{
		bSuffixRemove = false;
	}

	UpdateEnables();
}

void SAdvancedRenamerPanel::OnSuffixRemoveCharactersChanged(uint8 NewValue)
{
	RequestListViewRefresh();
}

void SAdvancedRenamerPanel::OnSuffixRemoveNumberCheckBoxChanged(ECheckBoxState NewState)
{
	bSuffixRemoveNumber = NewState == ECheckBoxState::Checked;

	UpdateEnables();
}

void SAdvancedRenamerPanel::OnSuffixNumberCheckBoxChanged(ECheckBoxState NewState)
{
	bSuffixNumber = NewState == ECheckBoxState::Checked;

	if (bSuffixNumber)
	{
		bSuffixRemoveNumber = true;
	}

	UpdateEnables();
}

void SAdvancedRenamerPanel::OnSuffixNumberStartChanged(int32 NewValue)
{
	RequestListViewRefresh();
}

void SAdvancedRenamerPanel::OnSuffixNumberStepChanged(int32 NewValue)
{
	RequestListViewRefresh();
}

void SAdvancedRenamerPanel::OnSearchReplacePlainTextCheckBoxChanged(ECheckBoxState NewState)
{
	bSearchReplacePlainText = NewState == ECheckBoxState::Checked;

	if (bSearchReplacePlainText)
	{
		bSearchReplaceRegex = false;
	}

	UpdateEnables();
}

void SAdvancedRenamerPanel::OnSearchReplaceRegexCheckBoxChanged(ECheckBoxState NewState)
{
	bSearchReplaceRegex = NewState == ECheckBoxState::Checked;

	if (bSearchReplaceRegex)
	{
		bSearchReplacePlainText = false;
	}

	UpdateEnables();
}

void SAdvancedRenamerPanel::OnSearchReplaceIgnoreCaseCheckBoxChanged(ECheckBoxState NewState)
{
	bSearchReplaceIgnoreCase = NewState == ECheckBoxState::Checked;

	UpdateEnables();
}

void SAdvancedRenamerPanel::OnSearchReplaceSearchTextChanged(const FText& NewText)
{
	RequestListViewRefresh();
}

void SAdvancedRenamerPanel::OnSearchReplaceReplaceTextChanged(const FText& NewText)
{
	RequestListViewRefresh();
}

TSharedRef<ITableRow> SAdvancedRenamerPanel::OnGenerateRowForList(FObjectRenamePreviewListItemPtr Item, 
	const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(SAdvancedRenamerPreviewListRow, SharedThis(this), OwnerTable, Item);
}

FReply SAdvancedRenamerPanel::OnListViewKeyDown(const FGeometry& MyGeometry, const FKeyEvent& KeyEvent)
{
	if (CommandList.IsValid() && CommandList->ProcessCommandBindings(KeyEvent))
	{
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

TSharedPtr<SWidget> SAdvancedRenamerPanel::GenerateListViewContextMenu()
{
	if (!RenamePreviewList.IsValid())
	{
		return nullptr;
	}

	if (RenamePreviewList->GetNumItemsSelected() == 0)
	{
		return nullptr;
	}

	FMenuBuilder MenuBuilder(true, CommandList.ToSharedRef());

	MenuBuilder.BeginSection("Actions", LOCTEXT("Actions", "Actions"));
	{
		MenuBuilder.AddMenuEntry(FGenericCommands::Get().Delete, NAME_None, LOCTEXT("RemoveObject", "Remove Object"));
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

FReply SAdvancedRenamerPanel::OnApplyButtonClicked()
{
	if (RenameObjects())
	{
		CloseWindow();
	}

	return FReply::Handled();
}

int32 SAdvancedRenamerPanel::Num() const
{
	return SharedProvider->Num();
}

bool SAdvancedRenamerPanel::IsValidIndex(int32 Index) const
{
	return SharedProvider->IsValidIndex(Index);
}

uint32 SAdvancedRenamerPanel::GetHash(int32 Index) const
{
	return SharedProvider->GetHash(Index);
}

FString SAdvancedRenamerPanel::GetOriginalName(int32 Index) const
{
	return SharedProvider->GetOriginalName(Index);
}

bool SAdvancedRenamerPanel::RemoveIndex(int32 Index)
{
	// Can fail during construction when indices that aren't renameable are removed from the provider before
	// they are added to ListData.
	if (ListData.IsValidIndex(Index))
	{
		ListData.RemoveAt(Index);
	}

	return SharedProvider->RemoveIndex(Index);
}

bool SAdvancedRenamerPanel::CanRename(int32 Index) const
{
	return SharedProvider->CanRename(Index);
}

bool SAdvancedRenamerPanel::ExecuteRename(int32 Index, const FString& NewName)
{
	return SharedProvider->ExecuteRename(Index, NewName);
}

#undef LOCTEXT_NAMESPACE
