// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimPreviewInstance.h"
#include "Editor.h"
#include "IAnimationEditor.h"
#include "IPersonaToolkit.h"
#include "PoseSearchDatabaseEditor.h"
#include "PoseSearchDatabaseViewModel.h"
#include "PoseSearchDebuggerDatabaseRowData.h"
#include "Animation/DebugSkelMeshComponent.h"
#include "Preferences/PersonaOptions.h"
#include "Styling/AppStyle.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Widgets/Input/SHyperlink.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "PoseSearchDebugger"

namespace UE::PoseSearch::DebuggerDatabaseColumns
{
	
/** Column struct to represent each column in the debugger database */
struct IColumn : TSharedFromThis<IColumn>
{
	explicit IColumn(int32 InSortIndex, bool InEnabled = true)
		: SortIndex(InSortIndex)
		, bEnabled(InEnabled)
	{
		ColumnId = FName(FString::Printf(TEXT("Column %d"), SortIndex));
	}

	virtual ~IColumn() = default;

	FName ColumnId;

	/** Sorted left to right based on this index */
	int32 SortIndex = 0;
	/** Current width, starts at 1 to be evenly spaced between all columns */
	float Width = 1.0f;
	/** Disabled selectively with view options */
	bool bEnabled = false;

	virtual FText GetLabel() const = 0;

	using FRowDataRef = TSharedRef<FDebuggerDatabaseRowData>;
	using FSortPredicate = TFunction<bool(const FRowDataRef&, const FRowDataRef&)>;

	virtual FSortPredicate GetSortPredicate() const = 0;

	virtual TSharedRef<SWidget> GenerateWidget(const FRowDataRef& RowData) const = 0;
};

/** Column struct to represent each column in the debugger database */
struct ITextColumn : IColumn
{
	explicit ITextColumn(int32 InSortIndex, bool InEnabled = true)
		: IColumn(InSortIndex, InEnabled)
	{
	}

	virtual ~ITextColumn() = default;
		
	virtual TSharedRef<SWidget> GenerateWidget(const FRowDataRef& RowData) const override
	{
		static FSlateFontInfo RowFont = FAppStyle::Get().GetFontStyle("DetailsView.CategoryTextStyle");
			
		return SNew(STextBlock)
            .Font(RowFont)
			.Text_Lambda([this, RowData]() -> FText { return GetRowText(RowData); })
            .Justification(ETextJustify::Center)
			.ColorAndOpacity_Lambda([this, RowData] { return GetColorAndOpacity(RowData); });
	}

protected:
	virtual FText GetRowText(const FRowDataRef& Row) const = 0;
	virtual FSlateColor GetColorAndOpacity(const FRowDataRef& Row) const
	{
		return FSlateColor(FLinearColor::White);
	}
};

struct FPoseIdx : ITextColumn
{
	using ITextColumn::ITextColumn;

	virtual FText GetLabel() const override { return LOCTEXT("ColumnLabelPoseIndex", "Index"); }

	virtual FSortPredicate GetSortPredicate() const override
	{
		return [](const FRowDataRef& Row0, const FRowDataRef& Row1) -> bool { return Row0->PoseIdx < Row1->PoseIdx; };
	}

	virtual FText GetRowText(const FRowDataRef& Row) const override
	{
		return FText::AsNumber(Row->PoseIdx, &FNumberFormattingOptions::DefaultNoGrouping());
	}
};

struct FDatabaseName : IColumn
{
	using IColumn::IColumn;

	virtual FText GetLabel() const override { return LOCTEXT("ColumnLabelDatabaseName", "Database"); }

	virtual FSortPredicate GetSortPredicate() const override
	{
		return [](const FRowDataRef& Row0, const FRowDataRef& Row1) -> bool { return Row0->DatabaseName < Row1->DatabaseName; };
	}

	virtual TSharedRef<SWidget> GenerateWidget(const FRowDataRef& RowData) const override
	{
		return SNew(SHyperlink)
			.Text_Lambda([RowData]() -> FText { return FText::FromString(RowData->DatabaseName); })
			.TextStyle(&FCoreStyle::Get().GetWidgetStyle<FTextBlockStyle>("SmallText"))
			.ToolTipText_Lambda([RowData]() -> FText
				{
					return FText::Format(
						LOCTEXT("DatabaseHyperlinkTooltipFormat", "Open database '{0}'"),
						FText::FromString(RowData->DatabasePath));
				})
			.OnNavigate_Lambda([RowData]()
				{
					UObject* Asset = nullptr;

					// Load asset
					if (UPackage* Package = LoadPackage(NULL, *RowData->DatabasePath, LOAD_NoRedirects))
					{
						Package->FullyLoad();

						const FString AssetName = FPaths::GetBaseFilename(RowData->DatabasePath);
						Asset = FindObject<UObject>(Package, *AssetName);
					}
					else
					{
						// Fallback for unsaved assets
						Asset = FindObject<UObject>(nullptr, *RowData->DatabasePath);
					}

					// Open editor
					if (Asset != nullptr)
					{
						if (UAssetEditorSubsystem* AssetEditorSS = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
						{
							AssetEditorSS->OpenEditorForAsset(Asset);
								
							if (IAssetEditorInstance* Editor = AssetEditorSS->FindEditorForAsset(Asset, true))
							{
								if (Editor->GetEditorName() == FName("PoseSearchDatabaseEditor"))
								{
									FDatabaseEditor* DatabaseEditor = static_cast<FDatabaseEditor*>(Editor);
									
									// Open asset paused and at specific time as seen on the pose search debugger.
									DatabaseEditor->SetSelectedAsset(RowData->DbAssetIdx);
									DatabaseEditor->GetViewModel()->SetPlayTime(RowData->AssetTime, false);
								}
							}
						}
					}
				});
	}
};

struct FAssetName : IColumn
{
	using IColumn::IColumn;

	virtual FText GetLabel() const override { return LOCTEXT("ColumnLabelAssetName", "Asset"); }
		
	virtual FSortPredicate GetSortPredicate() const override
	{
		return [](const FRowDataRef& Row0, const FRowDataRef& Row1) -> bool { return Row0->AssetName < Row1->AssetName; };
	}

	virtual TSharedRef<SWidget> GenerateWidget(const FRowDataRef& RowData) const override
	{
		return SNew(SHyperlink)
			.Text_Lambda([RowData]() -> FText { return FText::FromString(RowData->AssetName); })
			.TextStyle(&FCoreStyle::Get().GetWidgetStyle<FTextBlockStyle>("SmallText"))
			.ToolTipText_Lambda([RowData]() -> FText 
				{ 
					return FText::Format(
						LOCTEXT("AssetHyperlinkTooltipFormat", "Open asset '{0}'"), 
						FText::FromString(RowData->AssetPath)); 
				})
			.OnNavigate_Lambda([RowData]()
				{
					UObject* Asset = nullptr;

					// Load asset
					if (UPackage* Package = LoadPackage(NULL, *RowData->AssetPath, LOAD_NoRedirects))
					{
						Package->FullyLoad();

						const FString AssetName = FPaths::GetBaseFilename(RowData->AssetPath);
						Asset = FindObject<UObject>(Package, *AssetName);
					}
					else
					{
						// Fallback for unsaved assets
						Asset = FindObject<UObject>(nullptr, *RowData->AssetPath);
					}

					// Open editor
					if (Asset != nullptr)
					{
						if (UAssetEditorSubsystem* AssetEditorSS = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
						{
							AssetEditorSS->OpenEditorForAsset(Asset);
							
							if (IAssetEditorInstance* Editor = AssetEditorSS->FindEditorForAsset(Asset, true))
							{
								if (Editor->GetEditorName() == "AnimationEditor")
								{
									const IAnimationEditor* AnimationEditor = static_cast<IAnimationEditor*>(Editor);
									const UDebugSkelMeshComponent* PreviewComponent = AnimationEditor->GetPersonaToolkit()->GetPreviewMeshComponent();

									// Open asset paused and at specific time as seen on the pose search debugger.
									PreviewComponent->PreviewInstance->SetPosition(RowData->AssetTime);
									PreviewComponent->PreviewInstance->SetPlaying(false);
									PreviewComponent->PreviewInstance->SetBlendSpacePosition(RowData->BlendParameters);
								}
							}
						}
					}
				});
	}
};

struct FFrame : ITextColumn
{
	using ITextColumn::ITextColumn;

	virtual FText GetLabel() const override { return LOCTEXT("ColumnLabelFrame", "Frame"); }
		
	virtual FSortPredicate GetSortPredicate() const override
	{
		return [](const FRowDataRef& Row0, const FRowDataRef& Row1) -> bool { return Row0->AssetTime < Row1->AssetTime; };
	}

	virtual FText GetRowText(const FRowDataRef& Row) const override
	{
		FNumberFormattingOptions TimeFormattingOptions = FNumberFormattingOptions()
			.SetUseGrouping(false)
			.SetMaximumFractionalDigits(2);

		FNumberFormattingOptions PercentageFormattingOptions = FNumberFormattingOptions()
			.SetMaximumFractionalDigits(2);

		if (Row->AssetType == ESearchIndexAssetType::Sequence ||
			Row->AssetType == ESearchIndexAssetType::AnimComposite)
		{
			if (GetDefault<UPersonaOptions>()->bTimelineDisplayPercentage)
			{
				return FText::Format(
					FText::FromString("{0} ({1}) ({2})"),
					FText::AsNumber(Row->AnimFrame, &FNumberFormattingOptions::DefaultNoGrouping()),
					FText::AsNumber(Row->AssetTime, &TimeFormattingOptions),
					FText::AsPercent(Row->AnimPercentage, &PercentageFormattingOptions));
			}
			else
			{
				return FText::Format(
					FText::FromString("{0} ({1})"),
					FText::AsNumber(Row->AnimFrame, &FNumberFormattingOptions::DefaultNoGrouping()),
					FText::AsNumber(Row->AssetTime, &TimeFormattingOptions));
			}
		}
		else if (Row->AssetType == ESearchIndexAssetType::BlendSpace)
		{
			// There is no frame index associated with a blendspace
			return FText::Format(
				FText::FromString("({0})"),
				FText::AsNumber(Row->AssetTime, &TimeFormattingOptions));
		}
		else
		{
			return FText::FromString(TEXT("-"));
		}
	}
};

struct FCost : ITextColumn
{
	using ITextColumn::ITextColumn;

	virtual FText GetLabel() const override { return LOCTEXT("ColumnLabelCost", "Cost"); }
		
	virtual FSortPredicate GetSortPredicate() const override
	{
		return [](const FRowDataRef& Row0, const FRowDataRef& Row1) -> bool { return Row0->PoseCost < Row1->PoseCost; };
	}
		
	virtual FText GetRowText(const FRowDataRef& Row) const override
	{
		if (Row->PoseCost.IsValid())
		{
			return FText::AsNumber(Row->PoseCost.GetTotalCost());
		}
		else
		{
			return FText::FromString(TEXT("-"));
		}
    }

	virtual FSlateColor GetColorAndOpacity(const FRowDataRef& Row) const override
	{
		return Row->CostColor;
	}
};

struct FChannelBreakdownCostColumn : ITextColumn
{
	FChannelBreakdownCostColumn(int32 SortIndex, int32 InBreakdownCostIndex, const FText& InLabel)
		: ITextColumn(SortIndex)
		, Label(InLabel)
		, BreakdownCostIndex(InBreakdownCostIndex)
	{
	}

	virtual FText GetLabel() const override { return Label; }

	virtual FSortPredicate GetSortPredicate() const override
	{
		return [this](const FRowDataRef& Row0, const FRowDataRef& Row1) -> bool
		{
			return Row0->CostBreakdowns[BreakdownCostIndex] < Row1->CostBreakdowns[BreakdownCostIndex];
		};
	}

	virtual FText GetRowText(const FRowDataRef& Row) const override
	{
		const float LabelCost = Row->CostBreakdowns[BreakdownCostIndex];
		if (LabelCost != UE_MAX_FLT)
		{
			return FText::AsNumber(LabelCost);
		}
		return FText::FromString(TEXT("-"));
	}

	virtual FSlateColor GetColorAndOpacity(const FRowDataRef& Row) const override
	{
		return Row->CostBreakdownsColors[BreakdownCostIndex];
	}

	FText Label;
	int32 BreakdownCostIndex = INDEX_NONE;
};

#if WITH_EDITORONLY_DATA
struct FCostModifier : ITextColumn
{
	using ITextColumn::ITextColumn;

	virtual FText GetLabel() const override { return LOCTEXT("ColumnLabelCostModifier", "Bias"); }

	virtual FSortPredicate GetSortPredicate() const override
	{
		return [](const FRowDataRef& Row0, const FRowDataRef& Row1) -> bool { return Row0->PoseCost.GetCostAddend() < Row1->PoseCost.GetCostAddend(); };
	}

	virtual FText GetRowText(const FRowDataRef& Row) const override
	{
		return FText::AsNumber(Row->PoseCost.GetCostAddend());
	}
};
#endif // WITH_EDITORONLY_DATA

struct FMirrored : ITextColumn
{
	using ITextColumn::ITextColumn;

	virtual FText GetLabel() const override { return LOCTEXT("ColumnLabelMirrored", "Mirror"); }

	virtual FSortPredicate GetSortPredicate() const override
	{
		return [](const FRowDataRef& Row0, const FRowDataRef& Row1) -> bool { return Row0->bMirrored < Row1->bMirrored; };
	}

	virtual FText GetRowText(const FRowDataRef& Row) const override
	{
		return FText::Format(LOCTEXT("Mirrored", "{0}"), { Row->bMirrored } );
	}
};

struct FLooping : ITextColumn
{
	using ITextColumn::ITextColumn;

	virtual FText GetLabel() const override { return LOCTEXT("ColumnLabelLooping", "Loop"); }

	virtual FSortPredicate GetSortPredicate() const override
	{
		return [](const FRowDataRef& Row0, const FRowDataRef& Row1) -> bool { return Row0->bLooping < Row1->bLooping; };
	}

	virtual FText GetRowText(const FRowDataRef& Row) const override
	{
		return FText::Format(LOCTEXT("Looping", "{0}"), { Row->bLooping });
	}
};

struct FBlendParameters : ITextColumn
{
	using ITextColumn::ITextColumn;

	virtual FText GetLabel() const override { return LOCTEXT("ColumnLabelBlendParams", "Blend Params"); }

	virtual FSortPredicate GetSortPredicate() const override
	{
		return [](const FRowDataRef& Row0, const FRowDataRef& Row1) -> bool
		{
			return (
				Row0->BlendParameters[0] < Row1->BlendParameters[0] ||
				Row0->BlendParameters[1] < Row1->BlendParameters[1]);
		};
	}

	virtual FText GetRowText(const FRowDataRef& Row) const override
	{
		if (Row->AssetType == ESearchIndexAssetType::BlendSpace)
		{
			return FText::Format(
				LOCTEXT("Blend Parameters", "({0}, {1})"),
				FText::AsNumber(Row->BlendParameters[0]),
				FText::AsNumber(Row->BlendParameters[1]));
		}
		else
		{
			return FText::FromString(TEXT("-"));
		}
	}
};

struct FPoseCandidateFlags : ITextColumn
{
	using ITextColumn::ITextColumn;

	virtual FText GetLabel() const override { return LOCTEXT("ColumnLabelPoseCandidateFlags", "Flags"); }

	virtual FSortPredicate GetSortPredicate() const override
	{
		return [](const FRowDataRef& Row0, const FRowDataRef& Row1) -> bool { return Row0->PoseCandidateFlags < Row1->PoseCandidateFlags; };
	}

	virtual FText GetRowText(const FRowDataRef& Row) const override
	{
		if (EnumHasAnyFlags(Row->PoseCandidateFlags, EPoseCandidateFlags::AnyDiscardedMask))
		{
			auto AddDelimiter = [](FTextBuilder& TextBuilder, bool& bNeedDelimiter)
			{
				if (bNeedDelimiter)
				{
					TextBuilder.AppendLine(LOCTEXT("DiscardedBy_Delimiter", " | "));
				}
				bNeedDelimiter = true;
			};

			bool bNeedDelimiter = false;

			FTextBuilder TextBuilder;
			if (EnumHasAnyFlags(Row->PoseCandidateFlags, EPoseCandidateFlags::DiscardedBy_PoseJumpThresholdTime))
			{
				AddDelimiter(TextBuilder, bNeedDelimiter);
				TextBuilder.AppendLine(LOCTEXT("DiscardedBy_PoseJumpThresholdTime", "PoseJumpThresholdTime"));
			}
				
			if (EnumHasAnyFlags(Row->PoseCandidateFlags, EPoseCandidateFlags::DiscardedBy_PoseReselectHistory))
			{
				AddDelimiter(TextBuilder, bNeedDelimiter);
				TextBuilder.AppendLine(LOCTEXT("DiscardedBy_PoseReselectHistory", "PoseReselectHistory"));
			}

			if (EnumHasAnyFlags(Row->PoseCandidateFlags, EPoseCandidateFlags::DiscardedBy_BlockTransition))
			{
				AddDelimiter(TextBuilder, bNeedDelimiter);
				TextBuilder.AppendLine(LOCTEXT("DiscardedBy_BlockTransition", "BlockTransition"));
			}

			if (EnumHasAnyFlags(Row->PoseCandidateFlags, EPoseCandidateFlags::DiscardedBy_PoseFilter))
			{
				AddDelimiter(TextBuilder, bNeedDelimiter);
				TextBuilder.AppendLine(LOCTEXT("DiscardedBy_PoseFilter", "PoseFilter"));
			}

			return TextBuilder.ToText();
		}

		return FText::GetEmpty();
	}
};

} // namespace UE::PoseSearch::DebuggerDatabaseColumns

#undef LOCTEXT_NAMESPACE
