// Copyright Epic Games, Inc. All Rights Reserved.

#include "SMaterialEditorStatsWidget.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Images/SImage.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Fonts/FontMeasure.h"
#include "Styling/AppStyle.h"
#include "MaterialStatsGrid.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/StyleColors.h"

#define LOCTEXT_NAMESPACE "MaterialEditorStatsWidget"

const float SMaterialEditorStatsWidget::ColumnSizeSmall = 100.0f;
const float SMaterialEditorStatsWidget::ColumnSizeMedium = 150.0f;
const float SMaterialEditorStatsWidget::ColumnSizeLarge = 200.0f;
const float SMaterialEditorStatsWidget::ColumnSizeExtraLarge = 400.0f;

const FName SMaterialEditorStatsWidget::RegularFontStyle = TEXT("DataTableEditor.CellText");
const FName SMaterialEditorStatsWidget::BoldFontStyle = TEXT("RichTextBlock.Bold");

class SMaterialStatsViewRow : public SMultiColumnTableRow<TSharedPtr<int32>>
{
public:
	SLATE_BEGIN_ARGS(SMaterialStatsViewRow) {}
		/** The list item for this row */
		SLATE_ARGUMENT(TSharedPtr<int32>, PtrRowID)
		SLATE_ARGUMENT(TWeakPtr<FMaterialStats>, MaterialStatsWPtr)
	SLATE_END_ARGS()

	/** Construct function for this widget */
	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView)
	{
		PtrRowID = InArgs._PtrRowID;
		MaterialStatsWPtr = InArgs._MaterialStatsWPtr;

		SMultiColumnTableRow<TSharedPtr<int32>>::Construct(
			FSuperRowType::FArguments()
			.Style(FAppStyle::Get(), "TableView.Row"),
			InOwnerTableView
		);
	}

	/** Overridden from SMultiColumnTableRow.  Generates a widget for this column of the list view. */
	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override;

	FText GetTextForCell(const FName Name, const bool bToolTip) const;
	FSlateColor GetColorForCell(const FName Name) const;
	EHorizontalAlignment GetHAlignForCell(const FName Name) const;
	EVerticalAlignment GetVAlignForCell(const FName Name) const;

private:
	/** The item associated with this row of data */
	TSharedPtr<int32> PtrRowID;

	TWeakPtr<FMaterialStats> MaterialStatsWPtr;
};

TSharedRef<SWidget> SMaterialStatsViewRow::GenerateWidgetForColumn(const FName& ColumnName)
{
	EHorizontalAlignment HAlign = EHorizontalAlignment::HAlign_Fill;
	EVerticalAlignment VALign = EVerticalAlignment::VAlign_Top;

	FName UsedFontStyle = SMaterialEditorStatsWidget::GetRegulatFontStyleName();

	auto StatsPtr = MaterialStatsWPtr.Pin();
	if (StatsPtr.IsValid() && PtrRowID.IsValid())
	{
		const int32 RowID = *PtrRowID;

		auto Cell = StatsPtr->GetStatsGrid()->GetCell(RowID, ColumnName);

		UsedFontStyle = Cell->IsContentBold() ? SMaterialEditorStatsWidget::GetBoldFontStyleName() : SMaterialEditorStatsWidget::GetRegulatFontStyleName();
		HAlign = Cell->GetHorizontalAlignment();
		VALign = Cell->GetVerticalAlignment();
	}

	return SNew(SBox)
		.Padding(FMargin(4, 2, 4, 2))
		.HAlign(HAlign)
		.VAlign(VALign)
		[
			SNew(STextBlock)
			.TextStyle(FAppStyle::Get(), UsedFontStyle)
			.Text(this, &SMaterialStatsViewRow::GetTextForCell, ColumnName, false)
			.ToolTipText(this, &SMaterialStatsViewRow::GetTextForCell, ColumnName, true)
			.AutoWrapText(true)
		];
}

FText SMaterialStatsViewRow::GetTextForCell(const FName Name, const bool bToolTip) const
{
	FString CellContent;

	auto StatsPtr = MaterialStatsWPtr.Pin();
	if (StatsPtr.IsValid() && PtrRowID.IsValid())
	{
		const int32 RowID = *PtrRowID;

		auto Cell = StatsPtr->GetStatsGrid()->GetCell(RowID, Name);
		if (Cell.IsValid())
		{
			CellContent = bToolTip ? Cell->GetCellContentLong() : Cell->GetCellContent();
		}
	}

	FText FinalText = CellContent.Len() > 0 ? FText::FromString(CellContent) : FText::FromString(TEXT(""));
	return FinalText;
}

FSlateColor SMaterialStatsViewRow::GetColorForCell(const FName Name) const
{
	return FStyleColors::Foreground;
}

EHorizontalAlignment SMaterialStatsViewRow::GetHAlignForCell(const FName Name) const
{
	EHorizontalAlignment Align = HAlign_Center;

	auto StatsPtr = MaterialStatsWPtr.Pin();
	if (StatsPtr.IsValid() && PtrRowID.IsValid())
	{
		const int32 RowID = *PtrRowID;

		const auto Cell = StatsPtr->GetStatsGrid()->GetCell(RowID, Name);

		Align = Cell->GetHorizontalAlignment();
	}

	return Align;
}

EVerticalAlignment SMaterialStatsViewRow::GetVAlignForCell(const FName Name) const
{
	EVerticalAlignment Align = VAlign_Center;

	auto StatsPtr = MaterialStatsWPtr.Pin();
	if (StatsPtr.IsValid() && PtrRowID.IsValid())
	{
		const int32 RowID = *PtrRowID;

		const auto Cell = StatsPtr->GetStatsGrid()->GetCell(RowID, Name);

		Align = Cell->GetVerticalAlignment();
	}

	return Align;
}

float SMaterialEditorStatsWidget::GetColumnSize(const FName ColumnName) const
{
	float ColumnSize = ColumnSizeSmall;

	if (ColumnName == FMaterialStatsGrid::DescriptorColumnName)
	{
		ColumnSize = ColumnSizeMedium;
	}
	else if (ColumnName == FMaterialStatsGrid::ShaderColumnName)
	{
		ColumnSize = ColumnSizeLarge;
	}
	else
	{
		auto StatsPtr = MaterialStatsWPtr.Pin();
		if (StatsPtr.IsValid())
		{
			TSharedRef<FSlateFontMeasure> FontMeasureService = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
			FSlateFontInfo FontInfo = FAppStyle::GetFontStyle(BoldFontStyle);

			auto ArrRowIds = *StatsPtr->GetStatsGrid()->GetGridRowIDs();

			for (int32 i = 0; i < ArrRowIds.Num(); ++i)
			{
				if (ArrRowIds[i].IsValid())
				{
					const auto Cell = StatsPtr->GetStatsGrid()->GetCell(*ArrRowIds[i], ColumnName);

					const FString Content = Cell->GetCellContent();

					FVector2D FontMeasure = FontMeasureService->Measure(Content, FontInfo);

					ColumnSize = FMath::Clamp(FontMeasure.X, ColumnSize, ColumnSizeExtraLarge);
				}
			}
		}
	}

	return ColumnSize;
}

SHeaderRow::FColumn::FArguments SMaterialEditorStatsWidget::CreateColumnArgs(const FName ColumnName)
{
	FSlateColor Color = FStyleColors::AccentGreen;
	FString Content;
	FString ContentLong;
	auto StatsPtr = MaterialStatsWPtr.Pin();
	if (StatsPtr.IsValid())
	{
		Content = StatsPtr->GetStatsGrid()->GetColumnContent(ColumnName);
		ContentLong = StatsPtr->GetStatsGrid()->GetColumnContentLong(ColumnName);
		Color = StatsPtr->GetStatsGrid()->GetColumnColor(ColumnName);
	}

	const auto ColumnArgs = SHeaderRow::Column(ColumnName)
		.DefaultLabel(FText::FromString(Content))
		.HAlignHeader(EHorizontalAlignment::HAlign_Center)
		.ManualWidth(this, &SMaterialEditorStatsWidget::GetColumnSize, ColumnName)
		.HeaderContent()
		[
			SNew(STextBlock)
			.Margin(4.0f)
			.ColorAndOpacity(Color)
			.Text(FText::FromString(Content))
			.ToolTipText(FText::FromString(ContentLong))
		];

	return ColumnArgs;
}

void SMaterialEditorStatsWidget::InsertColumnAfter(const FName ColumnName, const FName PreviousColumn)
{
	int32 InsertIndex = -1;

	auto& ColumnArray = PlatformColumnHeader->GetColumns();
	auto Iterator = ColumnArray.CreateConstIterator();
	while (Iterator)
	{
		if (Iterator->ColumnId == PreviousColumn)
		{
			InsertIndex = Iterator.GetIndex() + 1;
			break;
		}

		++Iterator;
	}

	if (InsertIndex != -1)
	{
		const auto Column = CreateColumnArgs(ColumnName);
		PlatformColumnHeader->InsertColumn(Column, InsertIndex);
	}
}

void SMaterialEditorStatsWidget::AddColumn(const FName ColumnName)
{
	const auto Column = CreateColumnArgs(ColumnName);
	PlatformColumnHeader->AddColumn(Column);
}

void SMaterialEditorStatsWidget::RemoveColumn(const FName ColumnName)
{
	PlatformColumnHeader->RemoveColumn(ColumnName);
}

void SMaterialEditorStatsWidget::RebuildColumns()
{
	PlatformColumnHeader->ClearColumns();

	auto StatsManager = MaterialStatsWPtr.Pin();
	if (StatsManager.IsValid())
	{
		auto Grid = StatsManager->GetStatsGrid();

		if (Grid.IsValid())
		{
			auto ColumnNames = Grid->GetVisibleColumnNames();

			for (int32 i = 0; i < ColumnNames.Num(); ++i)
			{
				AddColumn(ColumnNames[i]);
			}
		}
	}
}

uint32 SMaterialEditorStatsWidget::CountSubPlatforms(EPlatformCategoryType Category) const
{
	uint32 PlatformCount = 0;
	const auto StatsPtr = MaterialStatsWPtr.Pin();

	if (!StatsPtr.IsValid())
	{
		return 0;
	}

	auto& PlatformsDB = StatsPtr->GetPlatformsTypeDB();
	auto* ArrPlaformsPtr = PlatformsDB.Find(Category);
	if (ArrPlaformsPtr == nullptr)
	{
		return 0;
	}

	auto& ArrPlatforms = *ArrPlaformsPtr;

	for (int32 i = 0; i < ArrPlatforms.Num(); ++i)
	{
		auto PlatformPtr = ArrPlatforms[i];
		if (!PlatformPtr.IsValid() || !PlatformPtr->IsStatsGridPresenceAllowed())
		{
			continue;
		}

		PlatformCount++;
	}

	return PlatformCount;

}

void SMaterialEditorStatsWidget::CreatePlatformMenus(FMenuBuilder& Builder, EPlatformCategoryType Category)
{
	const auto StatsPtr = MaterialStatsWPtr.Pin();

	if (!StatsPtr.IsValid())
	{
		return;
	}

	auto& PlatformsDB = StatsPtr->GetPlatformsTypeDB();
	auto* ArrPlaformsPtr = PlatformsDB.Find(Category);
	if (ArrPlaformsPtr == nullptr)
	{
		return;
	}

	auto& ArrPlatforms = *ArrPlaformsPtr;

	for (int32 i = 0; i < ArrPlatforms.Num(); ++i)
	{
		auto PlatformPtr = ArrPlatforms[i];
		if (!PlatformPtr.IsValid() || !PlatformPtr->IsStatsGridPresenceAllowed())
		{
			continue;
		}

		const FName PlatformName = PlatformPtr->GetPlatformName();

		// lambda function used to determine the check-state for the shader platform checkboxes
		auto Lamda_PlatformCheckState = [PlatformPtr = PlatformPtr]()
		{
			if (PlatformPtr.IsValid())
			{
				return PlatformPtr->IsPresentInGrid() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			}

			return ECheckBoxState::Unchecked;
		};

		// lambda used with shader platform checkboxes to add or remove selected shader platforms
		auto Lamda_PlatformFlipState = [WidgetPtr = this, PlatformPtr = PlatformPtr](const ECheckBoxState NewState)
		{
			auto MaterialStats = WidgetPtr->MaterialStatsWPtr.Pin();
			if (PlatformPtr.IsValid() && MaterialStats.IsValid())
			{
				const bool bSwitchValue = MaterialStats->SwitchShaderPlatformUseStats(PlatformPtr->GetPlatformShaderType());

				for (int32 q = 0; q < EMaterialQualityLevel::Num; ++q)
				{
					EMaterialQualityLevel::Type QualityLevel = static_cast<EMaterialQualityLevel::Type>(q);
					if (MaterialStats->GetStatsQualityFlag(QualityLevel))
					{
						const FName PlatformColumnName = FMaterialStatsGrid::MakePlatformColumnName(PlatformPtr, QualityLevel);

						if (bSwitchValue)
						{
							WidgetPtr->AddColumn(PlatformColumnName);
						}
						else
						{
							WidgetPtr->RemoveColumn(PlatformColumnName);
						}
					}
				}
			}

			WidgetPtr->RequestRefresh();
		};

		auto PlatformWidget = SNew(SCheckBox)			
			.OnCheckStateChanged_Lambda(Lamda_PlatformFlipState)
			.IsChecked_Lambda(Lamda_PlatformCheckState)
			.Content()
			[
				SNew(STextBlock)
				.Text(FText::FromName(PlatformName))
				.Margin(FMargin(2.0f, 2.0f, 4.0f, 2.0f))
			];

		Builder.AddMenuEntry(FUIAction(), PlatformWidget);
	}
}

void SMaterialEditorStatsWidget::CreatePlatformCategoryMenus(FMenuBuilder& Builder)
{
	for (int32 i = 0; i < (int32)EPlatformCategoryType::Num; ++i)
	{
		const EPlatformCategoryType PlatformType = static_cast<EPlatformCategoryType>(i);

		if (CountSubPlatforms(PlatformType) > 0)
		{
			auto PlatformTypeWidget = SNew(STextBlock)
				.Text(FText::FromString(FMaterialStatsUtils::GetPlatformTypeName(PlatformType)));

			Builder.AddSubMenu(PlatformTypeWidget, FNewMenuDelegate::CreateSP(this, &SMaterialEditorStatsWidget::CreatePlatformMenus, PlatformType));
		}
	}
}

void SMaterialEditorStatsWidget::OnFlipQualityState(const ECheckBoxState NewState, const EMaterialQualityLevel::Type QualityLevel)
{
	auto StatsPtr = MaterialStatsWPtr.Pin();
	if (StatsPtr.IsValid())
	{
		bool bSwitchValue = StatsPtr->SwitchStatsQualityFlag(QualityLevel);

		StatsPtr->GetStatsGrid()->OnQualitySettingChanged(QualityLevel);

		auto& PlatformDB = StatsPtr->GetPlatformsDB();
		for (auto Pair : PlatformDB)
		{
			TSharedPtr<FShaderPlatformSettings> Platform = Pair.Value;
			if (Platform->IsPresentInGrid())
			{
				const FName ColumnName = FMaterialStatsGrid::MakePlatformColumnName(Platform, QualityLevel);

				if (bSwitchValue)
				{
					// find insert spot, right after the a column used by the same platform, with other quality settings
					EMaterialQualityLevel::Type InsertAfterQuality = QualityLevel;

					for (int32 i = 0; i < EMaterialQualityLevel::Num; ++i)
					{
						if (StatsPtr->GetStatsQualityFlag((EMaterialQualityLevel::Type)i) && i != QualityLevel)
						{
							InsertAfterQuality = (EMaterialQualityLevel::Type)i;
							break;
						}
					}

					if (QualityLevel != InsertAfterQuality)
					{
						const FName PreviousColumnName = FMaterialStatsGrid::MakePlatformColumnName(Platform, InsertAfterQuality);
						InsertColumnAfter(ColumnName, PreviousColumnName);
					}
					else
					{
						AddColumn(ColumnName);
					}
				}
				else
				{
					RemoveColumn(ColumnName);
				}
			}
		}

		RequestRefresh();
	}
}

void SMaterialEditorStatsWidget::CreateQualityMenus(FMenuBuilder& Builder)
{
	for (int32 i = 0; i < EMaterialQualityLevel::Num; ++i)
	{
		auto QualityLevel = static_cast<EMaterialQualityLevel::Type>(i);

		auto Lamda_QualityCheckState = [QualityType = (EMaterialQualityLevel::Type)i, MaterialStatsWPtr = MaterialStatsWPtr]()
		{
			const auto StatsPtr = MaterialStatsWPtr.Pin();
			if (StatsPtr.IsValid())
			{
				return StatsPtr->GetStatsQualityFlag(QualityType) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			}

			return ECheckBoxState::Unchecked;
		};

		const FText QualitySettingName = FText::FromString(FMaterialStatsUtils::MaterialQualityToString(QualityLevel));

		auto QualityWidget = SNew(SCheckBox)
			.OnCheckStateChanged(this, &SMaterialEditorStatsWidget::OnFlipQualityState, QualityLevel)
			.IsChecked_Lambda(Lamda_QualityCheckState)
			.Content()
			[
				SNew(STextBlock)
				.Text(QualitySettingName)
			];

		Builder.AddMenuEntry(FUIAction(), QualityWidget);
	}
}

void SMaterialEditorStatsWidget::CreateGlobalQualityMenu(class FMenuBuilder& Builder)
{
	auto GlobalQualityWidget = SNew(STextBlock)
		.Text(LOCTEXT("GlobalQualitySettings", "Global Quality Settings"));

	Builder.AddSubMenu(GlobalQualityWidget, FNewMenuDelegate::CreateSP(this, &SMaterialEditorStatsWidget::CreateQualityMenus));
}

TSharedRef<SWidget> SMaterialEditorStatsWidget::GetSettingsButtonContent()
{
	FMenuBuilder Builder(false, nullptr);

	CreatePlatformCategoryMenus(Builder);
	Builder.AddMenuSeparator();
	CreateGlobalQualityMenu(Builder);

	return Builder.MakeWidget();
}

TSharedPtr<SWidget> SMaterialEditorStatsWidget::BuildMessageArea()
{
	MessageBoxWidget = SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight();

	return MessageBoxWidget;
}

void SMaterialEditorStatsWidget::AddWarningMessage(const FString& Message)
{
	MessageBoxWidget->AddSlot()
		.AutoHeight()
		.Padding(2.5, 2.5)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder")))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SImage)
					.Image(FAppStyle::GetBrush("MessageLog.Warning"))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.TextStyle(FAppStyle::Get(), TEXT("RichTextBlock.Bold"))
					.ColorAndOpacity(FStyleColors::Warning)
					.Text(FText::FromString(Message))
					.ToolTipText(FText::FromString(Message))
				]
			]
		];
}

void SMaterialEditorStatsWidget::ClearWarningMessages()
{
	MessageBoxWidget->ClearChildren();
}

void SMaterialEditorStatsWidget::Construct(const FArguments& InArgs)
{
	MaterialStatsWPtr = InArgs._MaterialStatsWPtr;
	
	const auto StatsPtr = MaterialStatsWPtr.Pin();

	if (!StatsPtr.IsValid())
	{
		return;
	}

	auto MessageArea = BuildMessageArea();

	SetVisibility(EVisibility::SelfHitTestInvisible);

	// construct default column headers
	PlatformColumnHeader = SNew(SHeaderRow);
	RebuildColumns();

	const auto VerticalScrollbar = SNew(SScrollBar)
		.Orientation(Orient_Vertical)
		.Thickness(FVector2D(11.0f, 11.0f));

	const auto HorizontalScrollbar = SNew(SScrollBar)
		.Orientation(Orient_Horizontal)
		.Thickness(FVector2D(11.0f, 11.0f));

	this->ChildSlot
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot() // this will contain the tool bar
		.Padding(0.0f)
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.FillWidth(1.0)
			[
				SNullWidget::NullWidget
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SComboButton)
				.ComboButtonStyle(FAppStyle::Get(), "SimpleComboButton")
				.ForegroundColor(FSlateColor::UseStyle())
				.ContentPadding(0)
				.OnGetMenuContent(this, &SMaterialEditorStatsWidget::GetSettingsButtonContent)
				.HasDownArrow(true)
				.ButtonContent()
				[
					SNew(SHorizontalBox)
					// Icon
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(SImage)
						.Image(FAppStyle::Get().GetBrush("LevelEditor.GameSettings"))
					]
					// Text
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(2, 0, 2, 0)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("SettingsButton", "Settings"))
					]
				]
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBorder)
			.Padding(0.0f)
			.HAlign(HAlign_Fill)
			.BorderImage(FAppStyle::Get().GetBrush(TEXT("Brushes.Recessed")))
			[
				MessageArea.ToSharedRef()
			]
		]
		+ SVerticalBox::Slot() // this will contain the stats grid
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::Get().GetBrush(TEXT("Brushes.Recessed")))
			.Padding(0.0f)
			.HAlign(HAlign_Fill)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				[
					SNew(SScrollBox)
					.Orientation(Orient_Vertical)
					.ExternalScrollbar(VerticalScrollbar)
					+ SScrollBox::Slot()
					[
						// ########## Material stats grid ##########
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.VAlign(VAlign_Fill)
						.AutoHeight()
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.HAlign(HAlign_Fill)
							.FillWidth(1.f)
							[
								SNew(SScrollBox)
								.Orientation(Orient_Horizontal)
								.ExternalScrollbar(HorizontalScrollbar)
								+ SScrollBox::Slot()
								[
									SAssignNew(MaterialInfoList, SListView<TSharedPtr<int32>>)
									.ExternalScrollbar(VerticalScrollbar)
									.ListItemsSource(StatsPtr->GetStatsGrid()->GetGridRowIDs())
									.OnGenerateRow(this, &SMaterialEditorStatsWidget::MakeMaterialInfoWidget)
									.Visibility(EVisibility::Visible)
									.SelectionMode(ESelectionMode::Single)
									.HeaderRow(PlatformColumnHeader)
								]
							]
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							HorizontalScrollbar
						]
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					VerticalScrollbar
				]
			]
		]
	];
}

TSharedRef<ITableRow> SMaterialEditorStatsWidget::MakeMaterialInfoWidget(const TSharedPtr<int32> PtrRowID, const TSharedRef<STableViewBase>& OwnerTable) const
{
	return SNew(SMaterialStatsViewRow, OwnerTable)
		.PtrRowID(PtrRowID)
		.MaterialStatsWPtr(MaterialStatsWPtr);
}

void SMaterialEditorStatsWidget::RequestRefresh()
{
	MaterialInfoList->RequestListRefresh();
}

FName SMaterialEditorStatsWidget::GetRegulatFontStyleName()
{
	return RegularFontStyle;
}

FName SMaterialEditorStatsWidget::GetBoldFontStyleName()
{
	return BoldFontStyle;
}

#undef LOCTEXT_NAMESPACE