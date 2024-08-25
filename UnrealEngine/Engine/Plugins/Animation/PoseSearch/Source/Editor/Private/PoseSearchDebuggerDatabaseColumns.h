// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimPreviewInstance.h"
#include "Animation/DebugSkelMeshComponent.h"
#include "Editor.h"
#include "IAnimationEditor.h"
#include "IPersonaToolkit.h"
#include "PoseSearchDatabaseEditor.h"
#include "PoseSearchDatabaseViewModel.h"
#include "PoseSearchDebuggerDatabaseRowData.h"
#include "PoseSearchDebuggerViewModel.h"
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
	virtual FText GetLabelTooltip() const { return GetLabel(); }
	
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
			.ToolTipText_Lambda([this, RowData]() -> FText { return GetRowToolTipText(RowData); })
            .Justification(ETextJustify::Center)
			.ColorAndOpacity_Lambda([this, RowData] { return GetColorAndOpacity(RowData); });
	}

protected:
	virtual FText GetRowText(const FRowDataRef& Row) const = 0;
	virtual FText GetRowToolTipText(const FRowDataRef& Row) const
	{
		return FText::GetEmpty();
	}
	virtual FSlateColor GetColorAndOpacity(const FRowDataRef& Row) const
	{
		return FSlateColor(FLinearColor::White);
	}
};

struct FPoseIdx : ITextColumn
{
	using ITextColumn::ITextColumn;

	virtual FText GetLabel() const override
	{
		return LOCTEXT("ColumnLabelPoseIndex", "Pose Id");
	}

	virtual FText GetLabelTooltip() const override
	{
		return LOCTEXT("ColumnLabelTooltipPoseIndex", "Index of the Pose in the Database");
	}

	virtual FSortPredicate GetSortPredicate() const override
	{
		return [](const FRowDataRef& Row0, const FRowDataRef& Row1) -> bool { return Row0->PoseIdx < Row1->PoseIdx; };
	}

	virtual FText GetRowText(const FRowDataRef& Row) const override
	{
		return FText::AsNumber(Row->PoseIdx, &FNumberFormattingOptions::DefaultNoGrouping());
	}
};

struct FAssetIdx : ITextColumn
{
	using ITextColumn::ITextColumn;

	virtual FText GetLabel() const override
	{
		return LOCTEXT("ColumnLabelAssetIndex", "Asset Id");
	}

	virtual FText GetLabelTooltip() const override
	{
		return LOCTEXT("ColumnLabelTooltipAssetIndex", "Index of the Asset in the Database");
	}

	virtual FSortPredicate GetSortPredicate() const override
	{
		return [](const FRowDataRef& Row0, const FRowDataRef& Row1) -> bool { return Row0->DbAssetIdx < Row1->DbAssetIdx; };
	}

	virtual FText GetRowText(const FRowDataRef& Row) const override
	{
		return FText::AsNumber(Row->DbAssetIdx, &FNumberFormattingOptions::DefaultNoGrouping());
	}
};


struct FDatabaseName : IColumn
{
	TSharedPtr<FDebuggerViewModel> DebuggerViewModel;

	FDatabaseName(int32 InSortIndex, TSharedPtr<FDebuggerViewModel> InDebuggerViewModel)
	: IColumn(InSortIndex)
	, DebuggerViewModel(InDebuggerViewModel)
	{
	}

	virtual FText GetLabel() const override
	{
		return LOCTEXT("ColumnLabelDatabaseName", "Database");
	}

	virtual FText GetLabelTooltip() const override
	{
		return LOCTEXT("ColumnLabelTooltipDatabaseName", "Database Name");
	}

	virtual FSortPredicate GetSortPredicate() const override
	{
		return [](const FRowDataRef& Row0, const FRowDataRef& Row1) -> bool { return Row0->SharedData->DatabaseName < Row1->SharedData->DatabaseName; };
	}

	virtual TSharedRef<SWidget> GenerateWidget(const FRowDataRef& RowData) const override
	{
		return SNew(SHyperlink)
		.Text_Lambda([RowData]() -> FText
		{
			return FText::FromString(RowData->SharedData->DatabaseName);
		})
		.TextStyle(&FCoreStyle::Get().GetWidgetStyle<FTextBlockStyle>("SmallText"))
		.ToolTipText_Lambda([RowData]() -> FText
		{
			return FText::Format(
				LOCTEXT("DatabaseHyperlinkTooltipFormat", "Open database '{0}'"),
				FText::FromString(RowData->SharedData->DatabasePath));
		})
		.OnNavigate_Lambda([this, RowData]()
		{
			UObject* Asset = nullptr;

			// Load asset
			if (UPackage* Package = LoadPackage(NULL, *RowData->SharedData->DatabasePath, LOAD_NoRedirects))
			{
				Package->FullyLoad();

				const FString AssetName = FPaths::GetBaseFilename(RowData->SharedData->DatabasePath);
				Asset = FindObject<UObject>(Package, *AssetName);
			}
			else
			{
				// Fallback for unsaved assets
				Asset = FindObject<UObject>(nullptr, *RowData->SharedData->DatabasePath);
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
							DatabaseEditor->SetSelectedPoseIdx(RowData->PoseIdx, DebuggerViewModel->GetDrawQuery(), RowData->SharedData->QueryVector);
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

	virtual FText GetLabel() const override
	{
		return LOCTEXT("ColumnLabelAssetName", "Asset");
	}
	
	virtual FText GetLabelTooltip() const override
	{
		return LOCTEXT("ColumnLabelTooltipAssetName", "Animation Asset Name");
	}

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

	virtual FText GetLabel() const override
	{
		return LOCTEXT("ColumnLabelFrame", "Frame");
	}
	
	virtual FText GetLabelTooltip() const override
	{
		return LOCTEXT("ColumnLabelTooltipFrame", "Frame number from the start of the Animation Asset");
	}

	virtual FSortPredicate GetSortPredicate() const override
	{
		return [](const FRowDataRef& Row0, const FRowDataRef& Row1) -> bool { return Row0->AnimFrame < Row1->AnimFrame; };
	}

	virtual FText GetRowText(const FRowDataRef& Row) const override
	{
		return FText::AsNumber(Row->AnimFrame, &FNumberFormattingOptions::DefaultNoGrouping());
	}
};

struct FTime : ITextColumn
{
	using ITextColumn::ITextColumn;

	virtual FText GetLabel() const override
	{
		return LOCTEXT("ColumnLabelTime", "Time");
	}

	virtual FText GetLabelTooltip() const override
	{
		return LOCTEXT("ColumnLabelTooltipTime", "Time in seconds from the start of the Animation Asset");
	}

	virtual FSortPredicate GetSortPredicate() const override
	{
		return [](const FRowDataRef& Row0, const FRowDataRef& Row1) -> bool { return Row0->AssetTime < Row1->AssetTime; };
	}

	virtual FText GetRowText(const FRowDataRef& Row) const override
	{
		return FText::AsNumber(Row->AssetTime, &FNumberFormattingOptions().SetUseGrouping(false).SetMaximumFractionalDigits(2));
	}
};

struct FPercentage : ITextColumn
{
	using ITextColumn::ITextColumn;

	virtual FText GetLabel() const override
	{
		return LOCTEXT("ColumnLabelPercentage", "Percent");
	}

	virtual FText GetLabelTooltip() const override
	{
		return LOCTEXT("ColumnLabelTooltipPercentage", "Time in percentage from the start of the Animation Asset");
	}

	virtual FSortPredicate GetSortPredicate() const override
	{
		return [](const FRowDataRef& Row0, const FRowDataRef& Row1) -> bool { return Row0->AnimPercentage < Row1->AnimPercentage; };
	}

	virtual FText GetRowText(const FRowDataRef& Row) const override
	{
		return FText::AsPercent(Row->AnimPercentage, &FNumberFormattingOptions().SetMaximumFractionalDigits(2));
	}
};

struct FCost : ITextColumn
{
	using ITextColumn::ITextColumn;

	virtual FText GetLabel() const override
	{
		return LOCTEXT("ColumnLabelCost", "Cost");
	}
	
	virtual FText GetLabelTooltip() const override
	{
		return LOCTEXT("ColumnLabelTooltipCost", "Total Cost of the associated Pose");
	}

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

struct FPCACost : ITextColumn
{
	using ITextColumn::ITextColumn;

	virtual FText GetLabel() const override
	{
		return LOCTEXT("ColumnLabelPCACost", "PCA Cost");
	}
	
	virtual FText GetLabelTooltip() const override
	{
		return LOCTEXT("ColumnLabelTooltipPCACost", "Total PCA Cost of the associated Pose");
	}

	virtual FSortPredicate GetSortPredicate() const override
	{
		return [](const FRowDataRef& Row0, const FRowDataRef& Row1) -> bool { return Row0->PosePCACost < Row1->PosePCACost; };
	}
		
	virtual FText GetRowText(const FRowDataRef& Row) const override
	{
		return FText::AsNumber(Row->PosePCACost);
    }

	virtual FSlateColor GetColorAndOpacity(const FRowDataRef& Row) const override
	{
		return Row->PCACostColor;
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

	virtual FText GetLabel() const override
	{
		return Label;
	}

	virtual FText GetLabelTooltip() const override
	{
		return FText::Format(LOCTEXT("ColumnLabelTooltipChannelBreakdownCost", "Breakdown Cost for the Channel '{0}'"), Label);
	}

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

struct FCostModifier : ITextColumn
{
	using ITextColumn::ITextColumn;

	virtual FText GetLabel() const override
	{
		return LOCTEXT("ColumnLabelCostModifier", "Bias");
	}

	virtual FText GetLabelTooltip() const override
	{
		return LOCTEXT("ColumnLabelTooltipCostModifier", "Total Cost for all the Bias contributions");
	}

	virtual FSortPredicate GetSortPredicate() const override
	{
		return [](const FRowDataRef& Row0, const FRowDataRef& Row1) -> bool { return Row0->PoseCost.GetCostAddend() < Row1->PoseCost.GetCostAddend(); };
	}

	virtual FText GetRowText(const FRowDataRef& Row) const override
	{
		return FText::AsNumber(Row->PoseCost.GetCostAddend());
	}
};

struct FMirrored : ITextColumn
{
	using ITextColumn::ITextColumn;

	virtual FText GetLabel() const override
	{
		return LOCTEXT("ColumnLabelMirrored", "Mirror");
	}

	virtual FText GetLabelTooltip() const override
	{
		return LOCTEXT("ColumnLabelTooltipMirrored", "Mirror state of the associated Pose");
	}

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

	virtual FText GetLabel() const override
	{
		return LOCTEXT("ColumnLabelLooping", "Loop");
	}

	virtual FText GetLabelTooltip() const override
	{
		return LOCTEXT("ColumnLabelTooltipLooping", "Loop state of the associated Pose");
	}

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

	virtual FText GetLabel() const override
	{
		return LOCTEXT("ColumnLabelBlendParams", "Blend Params");
	}

	virtual FText GetLabelTooltip() const override
	{
		return LOCTEXT("ColumnLabelBlendTooltipParams", "Blend Params used to sample the associated BlendSpace asset");
	}

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
		return FText::Format(LOCTEXT("Blend Parameters", "{0}, {1}"), FText::AsNumber(Row->BlendParameters[0]), FText::AsNumber(Row->BlendParameters[1]));
	}
};

struct FPoseCandidateFlags : ITextColumn
{
	using ITextColumn::ITextColumn;

	virtual FText GetLabel() const override
	{
		return LOCTEXT("ColumnLabelPoseCandidateFlags", "Flags");
	}

	virtual FText GetLabelTooltip() const override
	{
		return LOCTEXT("ColumnLabelTooltipPoseCandidateFlags", "Flags indicating why a Pose has been discarded");
	}

	virtual FSortPredicate GetSortPredicate() const override
	{
		return [](const FRowDataRef& Row0, const FRowDataRef& Row1) -> bool { return Row0->PoseCandidateFlags < Row1->PoseCandidateFlags; };
	}

	virtual FText GetRowText(const FRowDataRef& Row) const override
	{
		if (EnumHasAnyFlags(Row->PoseCandidateFlags, EPoseCandidateFlags::AnyDiscardedMask))
		{
			FString Sring;

			if (EnumHasAnyFlags(Row->PoseCandidateFlags, EPoseCandidateFlags::DiscardedBy_PoseJumpThresholdTime))
			{
				Sring.Append("J ");
			}

			if (EnumHasAnyFlags(Row->PoseCandidateFlags, EPoseCandidateFlags::DiscardedBy_PoseReselectHistory))
			{
				Sring.Append("H ");
			}
			
			if (EnumHasAnyFlags(Row->PoseCandidateFlags, EPoseCandidateFlags::DiscardedBy_BlockTransition))
			{
				Sring.Append("B ");
			}
			
			if (EnumHasAnyFlags(Row->PoseCandidateFlags, EPoseCandidateFlags::DiscardedBy_PoseFilter))
			{
				Sring.Append("F ");
			}

			if (EnumHasAnyFlags(Row->PoseCandidateFlags, EPoseCandidateFlags::DiscardedBy_AssetIdxFilter))
			{
				Sring.Append("A ");
			}
			
			if (EnumHasAnyFlags(Row->PoseCandidateFlags, EPoseCandidateFlags::DiscardedBy_Search))
			{
				Sring.Append("S ");
			}

			return FText::FromString(Sring);
		}

		return FText::GetEmpty();
	}

	virtual FText GetRowToolTipText(const FRowDataRef& Row) const override
	{
		if (EnumHasAnyFlags(Row->PoseCandidateFlags, EPoseCandidateFlags::AnyDiscardedMask))
		{
			FTextBuilder TextBuilder;
			TextBuilder.AppendLine(LOCTEXT("DiscardedBy_Reason_Tooltip", "Pose discarded because of:"));

			if (EnumHasAnyFlags(Row->PoseCandidateFlags, EPoseCandidateFlags::DiscardedBy_PoseJumpThresholdTime))
			{
				TextBuilder.AppendLine(LOCTEXT("DiscardedBy_PoseJumpThresholdTime_Tooltip", "(J) Pose Jump Threshold Time"));
			}

			if (EnumHasAnyFlags(Row->PoseCandidateFlags, EPoseCandidateFlags::DiscardedBy_PoseReselectHistory))
			{
				TextBuilder.AppendLine(LOCTEXT("DiscardedBy_PoseReselectHistory_Tooltip", "(H) Pose Reselect History"));
			}

			if (EnumHasAnyFlags(Row->PoseCandidateFlags, EPoseCandidateFlags::DiscardedBy_BlockTransition))
			{
				TextBuilder.AppendLine(LOCTEXT("DiscardedBy_BlockTransition_Tooltip", "(B) Block Transition"));
			}

			if (EnumHasAnyFlags(Row->PoseCandidateFlags, EPoseCandidateFlags::DiscardedBy_PoseFilter))
			{
				TextBuilder.AppendLine(LOCTEXT("DiscardedBy_PoseFilter_Tooltip", "(F) Filter"));
			}

			if (EnumHasAnyFlags(Row->PoseCandidateFlags, EPoseCandidateFlags::DiscardedBy_AssetIdxFilter))
			{
				TextBuilder.AppendLine(LOCTEXT("DiscardedBy_AssetIdxFilter_Tooltip", "(A) Asset Idx Filter"));
			}
			
			if (EnumHasAnyFlags(Row->PoseCandidateFlags, EPoseCandidateFlags::DiscardedBy_Search))
			{
				TextBuilder.AppendLine(LOCTEXT("DiscardedBy_Search_Tooltip", "(S) Search"));
			}

			return TextBuilder.ToText();
		}

		return FText::GetEmpty();
	}
};

} // namespace UE::PoseSearch::DebuggerDatabaseColumns

#undef LOCTEXT_NAMESPACE
