// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetPlacementPaletteItem.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SToolTip.h"
#include "Widgets/Input/SCheckBox.h"
#include "Modules/ModuleManager.h"
#include "Widgets/Images/SImage.h"
#include "Styling/AppStyle.h"

#include "AssetThumbnail.h"
#include "IAssetTools.h"
#include "IAssetTypeActions.h"
#include "AssetToolsModule.h"
#include "FoliageType.h"
#include "SAssetPlacementPalette.h"
#include "Engine/Blueprint.h"
#include "Factories/AssetFactoryInterface.h"
#include "AssetPlacementSettings.h"
#include "Instances/InstancedPlacementClientInfo.h"

#define LOCTEXT_NAMESPACE "AssetPlacementMode"

FPaletteItemUIInfo::FPaletteItemUIInfo(const FAssetData& InAssetData, const UPlacementPaletteClient* InPaletteItem)
	: AssetData(InAssetData)
	, SettingsObject(InPaletteItem ? InPaletteItem->SettingsObject.Get() : nullptr)
{
	check(AssetData.IsValid());
}

FAssetPlacementPaletteItemModel::FAssetPlacementPaletteItemModel(const FAssetData& InAssetData, const UPlacementPaletteClient* InPaletteItem, TSharedRef<SAssetPlacementPalette> InParentPalette, TSharedPtr<FAssetThumbnailPool> InThumbnailPool)
	: TypeInfo(MakeShared<FPaletteItemUIInfo>(InAssetData, InPaletteItem))
	, AssetPalette(InParentPalette)
{
	check(TypeInfo);
	DisplayFName = TypeInfo->AssetData.AssetName;

	int32 MaxThumbnailSize = PlacementPaletteConstants::ThumbnailSizeRange.Max;
	TSharedPtr<FAssetThumbnail> Thumbnail = MakeShared<FAssetThumbnail>(TypeInfo->AssetData, MaxThumbnailSize, MaxThumbnailSize, InThumbnailPool);
	
	FAssetThumbnailConfig ThumbnailConfig;
	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
	if (!IsBlueprint())
	{
		AssetTypeActions = AssetToolsModule.Get().GetAssetTypeActionsForClass(TypeInfo->AssetData.GetClass());
	}
	else
	{
		AssetTypeActions = AssetToolsModule.Get().GetAssetTypeActionsForClass(UBlueprint::StaticClass());
	}

	if (AssetTypeActions.IsValid())
	{
		ThumbnailConfig.AssetTypeColorOverride = AssetTypeActions.Pin()->GetTypeColor();
	}

	ThumbnailWidget = Thumbnail->MakeThumbnailWidget(ThumbnailConfig);
}

FAssetPlacementUIInfoPtr FAssetPlacementPaletteItemModel::GetTypeUIInfo() const
{
	return TypeInfo;
}

TSharedRef<SWidget> FAssetPlacementPaletteItemModel::GetThumbnailWidget() const
{
	return ThumbnailWidget.ToSharedRef();
}

TSharedRef<SToolTip> FAssetPlacementPaletteItemModel::CreateTooltipWidget() const
{
	return 
		SNew(SToolTip)
		.TextMargin(1)
		.BorderImage(FAppStyle::GetBrush("ContentBrowser.TileViewTooltip.ToolTipBorder"))
		.Visibility(this, &FAssetPlacementPaletteItemModel::GetTooltipVisibility)
		[
			SNew(SBorder)
			.Padding(3.f)
			.BorderImage(FAppStyle::GetBrush("ContentBrowser.TileViewTooltip.NonContentBorder"))
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SBorder)
					.Padding(FMargin(6.f))
					.HAlign(HAlign_Left)
					.BorderImage(FAppStyle::GetBrush("ContentBrowser.TileViewTooltip.ContentBorder"))
					[
						SNew(STextBlock)
						.Text(FText::FromName(DisplayFName))
						.Font(FAppStyle::GetFontStyle("ContentBrowser.TileViewTooltip.NameFont"))
						.HighlightText(this, &FAssetPlacementPaletteItemModel::GetPaletteSearchText)
					]
				]
				
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FMargin(0.f, 3.f, 0.f, 0.f))
				[
					
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(FMargin(0.f, 0.f, 3.f, 0.f))
					[
						SNew(SBorder)
						.Padding(6.f)
						.HAlign(HAlign_Center)
						.Visibility(this, &FAssetPlacementPaletteItemModel::GetTooltipThumbnailVisibility)
						.BorderImage(FAppStyle::GetBrush("ContentBrowser.TileViewTooltip.ContentBorder"))
						[
							SNew(SBox)
							.HeightOverride(64.f)
							.WidthOverride(64.f)
							[
								GetThumbnailWidget()
							]
						]
					]

					+ SHorizontalBox::Slot()
					[
						SNew(SBorder)
						.Padding(6.f)
						.BorderImage(FAppStyle::GetBrush("ContentBrowser.TileViewTooltip.ContentBorder"))
						[
							SNew(SVerticalBox)

							+ SVerticalBox::Slot()
							.Padding(0, 1)
							.AutoHeight()
							[
								SNew(SHorizontalBox)
								
								+ SHorizontalBox::Slot()
								.AutoWidth()
								[
									SNew(STextBlock)
									.Text(LOCTEXT("SourceAssetTypeHeading", "Source Asset Type: "))
									.ColorAndOpacity(FSlateColor::UseSubduedForeground())
								]

								+ SHorizontalBox::Slot()
								.AutoWidth()
								[
									SNew(STextBlock)
									.Text(this, &FAssetPlacementPaletteItemModel::GetSourceAssetTypeText)
								]
							]
						]
					]					
				]
			]
		];
}

FName FAssetPlacementPaletteItemModel::GetDisplayFName() const
{
	return DisplayFName;
}

FText FAssetPlacementPaletteItemModel::GetPaletteSearchText() const
{
	if (AssetPalette.IsValid())
	{
		return AssetPalette.Pin()->GetSearchText();
	}
	else
	{
		return FText();
	}
}

bool FAssetPlacementPaletteItemModel::IsBlueprint() const
{
	return TypeInfo && (TypeInfo->AssetData.GetClass()->GetDefaultObject()->IsA<UBlueprint>() || (TypeInfo->AssetData.GetClass()->ClassGeneratedBy != nullptr));
}

bool FAssetPlacementPaletteItemModel::IsAsset() const
{
	return TypeInfo && TypeInfo->AssetData.IsValid();
}

EVisibility FAssetPlacementPaletteItemModel::GetTooltipVisibility() const
{
	return (AssetPalette.IsValid() && AssetPalette.Pin()->ShouldShowTooltips()) ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed;
}

EVisibility FAssetPlacementPaletteItemModel::GetTooltipThumbnailVisibility() const
{
	return (AssetPalette.IsValid() && AssetPalette.Pin()->IsActiveViewMode(SAssetPlacementPalette::EViewMode::Tree)) ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed;
}

FText FAssetPlacementPaletteItemModel::GetSourceAssetTypeText() const
{
	if (AssetTypeActions.IsValid())
	{
		return AssetTypeActions.Pin()->GetName();
	}

	if (!TypeInfo->AssetData.AssetClassPath.IsNull())
	{
		return FText::FromName(TypeInfo->AssetData.AssetClassPath.GetAssetName());
	}

	return FText::FromName(TypeInfo->AssetData.AssetName);
}

////////////////////////////////////////////////
// SAssetPlacementPaletteItemTile
////////////////////////////////////////////////

const float SAssetPlacementPaletteItemTile::MinScaleForOverlayItems = 0.2f;

void SAssetPlacementPaletteItemTile::Construct(const FArguments& InArgs, TSharedRef<STableViewBase> InOwnerTableView, TSharedPtr<FAssetPlacementPaletteItemModel>& InModel)
{
	Model = InModel;

	STableRow<FAssetPlacementUIInfoPtr>::Construct(
		STableRow<FAssetPlacementUIInfoPtr>::FArguments()
		.Style(FAppStyle::Get(), "ContentBrowser.AssetListView.ColumnListTableRow")
		.Padding(1.f)
		.Content()
		[
			SNew(SOverlay)
			.ToolTip(Model->CreateTooltipWidget())
			
			// Thumbnail
			+ SOverlay::Slot()
			[
				SNew(SBorder)
				.Padding(4.f)
				.BorderImage(FAppStyle::GetBrush("ContentBrowser.ThumbnailShadow"))
				.ForegroundColor(FLinearColor::White)
				.ColorAndOpacity(FLinearColor::White)
				[
					Model->GetThumbnailWidget()
				]
			]
		], InOwnerTableView);
}

////////////////////////////////////////////////
// SAssetPlacementPaletteItemRow
////////////////////////////////////////////////

void SAssetPlacementPaletteItemRow::Construct(const FArguments& InArgs, TSharedRef<STableViewBase> InOwnerTableView, TSharedPtr<FAssetPlacementPaletteItemModel>& InModel)
{
	Model = InModel;
	SMultiColumnTableRow<FAssetPlacementUIInfoPtr>::Construct(FSuperRowType::FArguments(), InOwnerTableView);

	SetToolTip(Model->CreateTooltipWidget());
}

TSharedRef<SWidget> SAssetPlacementPaletteItemRow::GenerateWidgetForColumn(const FName& ColumnName)
{
	TSharedPtr<SWidget> TableRowContent = SNullWidget::NullWidget;

	if (ColumnName == AssetPlacementPaletteTreeColumns::ColumnID_Type)
	{
		TableRowContent =
			SNew(SHorizontalBox)
			
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SExpanderArrow, SharedThis(this))
			]
			
			+SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(STextBlock)
				.Text(FText::FromName(Model->GetDisplayFName()))
				.HighlightText(Model.ToSharedRef(), &FAssetPlacementPaletteItemModel::GetPaletteSearchText)
			];
	}

	return TableRowContent.ToSharedRef();
}

#undef LOCTEXT_NAMESPACE
