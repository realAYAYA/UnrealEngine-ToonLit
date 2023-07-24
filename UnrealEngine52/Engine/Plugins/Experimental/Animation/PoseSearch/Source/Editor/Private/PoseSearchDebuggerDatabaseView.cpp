// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearchDebuggerDatabaseView.h"
#include "Algo/AllOf.h"
#include "Animation/AnimComposite.h"
#include "Animation/AnimSequence.h"
#include "InstancedStruct.h"
#include "PoseSearchDebuggerDatabaseRow.h"
#include "PoseSearchDebuggerView.h"
#include "PoseSearchDebuggerViewModel.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "PoseSearch/PoseSearchDerivedData.h"
#include "PoseSearch/PoseSearchFeatureChannel.h"
#include "PoseSearch/PoseSearchSchema.h"
#include "Trace/PoseSearchTraceProvider.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SScrollBar.h"
#include "Widgets/Views/SListView.h"

#define LOCTEXT_NAMESPACE "PoseSearchDebugger"

namespace UE::PoseSearch
{

void SDebuggerDatabaseView::Update(const FTraceMotionMatchingStateMessage& State)
{
	class SCostBreakDownData
	{
	public:
		SCostBreakDownData(const TArray<FTraceMotionMatchingStateDatabaseEntry>& DatabaseEntries, bool bIsVerbose)
		{
			// processing all the DatabaseEntries to collect the LabelToChannels
			for (const FTraceMotionMatchingStateDatabaseEntry& DbEntry : DatabaseEntries)
			{
				const UPoseSearchDatabase* Database = FTraceMotionMatchingState::GetObjectFromId<UPoseSearchDatabase>(DbEntry.DatabaseId);
				if (FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(Database, ERequestAsyncBuildFlag::ContinueRequest))
				{
					for (const TObjectPtr<UPoseSearchFeatureChannel>& ChannelPtr : Database->Schema->Channels)
					{
						if (const UPoseSearchFeatureChannel* Channel = ChannelPtr.Get())
						{
							AnalyzeChannelRecursively(Channel, bIsVerbose);
						}
					}
				}
			}
		}

		void ProcessData(TArray<TSharedRef<FDebuggerDatabaseRowData>>& InOutUnfilteredDatabaseRows) const
		{
			for (TSharedRef<FDebuggerDatabaseRowData>& UnfilteredDatabaseRowRef : InOutUnfilteredDatabaseRows)
			{
				FDebuggerDatabaseRowData& UnfilteredDatabaseRow = UnfilteredDatabaseRowRef.Get();
				UnfilteredDatabaseRow.CostBreakdowns.AddDefaulted(LabelToChannels.Num());

				for (int32 LabelToChannelIndex = 0; LabelToChannelIndex < LabelToChannels.Num(); ++LabelToChannelIndex)
				{
					const FLabelToChannels& LabelToChannel = LabelToChannels[LabelToChannelIndex];

					// there should only be at most one channel per schema with the unique label,
					// but we'll keep this generic allowing multiple channels from the same schema having the same label.
					// the cost will be the sum of all the channels cost
					float CostBreakdown = 0.f;
					for (const UPoseSearchFeatureChannel* Channel : LabelToChannel.Channels)
					{
						// checking if the row is associated to the Channel
						if (UnfilteredDatabaseRow.SourceDatabase->Schema == Channel->GetSchema())
						{
							CostBreakdown += ArraySum(UnfilteredDatabaseRow.CostVector, Channel->GetChannelDataOffset(), Channel->GetChannelCardinality());
						}
					}
					UnfilteredDatabaseRow.CostBreakdowns[LabelToChannelIndex] = CostBreakdown;
				}
			}
		}

		bool AreLabelsEqualTo(const TArray<FText>& OtherLabels) const
		{
			if (LabelToChannels.Num() != OtherLabels.Num())
			{
				return false;
			}

			for (int32 i = 0; i < LabelToChannels.Num(); ++i)
			{
				if (!LabelToChannels[i].Label.EqualTo(OtherLabels[i]))
				{
					return false;
				}
			}

			return true;
		}

		const TArray<FText> GetLabels() const
		{
			TArray<FText> Labels;
			for (const FLabelToChannels& LabelToChannel : LabelToChannels)
			{
				Labels.Add(LabelToChannel.Label);
			}
			return Labels;
		}

	private:
		void AnalyzeChannelRecursively(const UPoseSearchFeatureChannel* Channel, bool bIsVerbose)
		{
			const FText Label = FText::FromString(Channel->GetLabel());

			bool bLabelFound = false;
			for (int32 i = 0; i < LabelToChannels.Num(); ++i)
			{
				if (LabelToChannels[i].Label.EqualTo(Label))
				{
					LabelToChannels[i].Channels.AddUnique(Channel);
					bLabelFound = true;
				}
			}
			if (!bLabelFound)
			{
				LabelToChannels.AddDefaulted();
				LabelToChannels.Last().Label = Label;
				LabelToChannels.Last().Channels.Add(Channel);
			}

			if (bIsVerbose)
			{
				for (const TObjectPtr<UPoseSearchFeatureChannel>& SubChannelPtr : Channel->GetSubChannels())
				{
					if (const UPoseSearchFeatureChannel* SubChannel = SubChannelPtr.Get())
					{
						AnalyzeChannelRecursively(SubChannel, bIsVerbose);
					}
				}
			}
		}

		static float ArraySum(TConstArrayView<float> View, int32 StartIndex, int32 Offset)
		{
			float Sum = 0.f;
			const int32 EndIndex = StartIndex + Offset;
			for (int i = StartIndex; i < EndIndex; ++i)
			{
				Sum += View[i];
			}
			return Sum;
		}

		struct FLabelToChannels
		{
			FText Label;
			TArray<const UPoseSearchFeatureChannel*> Channels; // NoTe: channels can be from different schemas
		};
		TArray<FLabelToChannels> LabelToChannels;
	};

	// row cost color palette
	static const FLinearColor DiscardedRowColor(0.314f, 0.314f, 0.314f); // darker gray
	static const FLinearColor BestScoreRowColor = FLinearColor::Green;
	static const FLinearColor WorstScoreRowColor = FLinearColor::Red;

	using namespace DebuggerDatabaseColumns;

	bool bIsVerbose = false;
	auto DebuggerView = ParentDebuggerViewPtr.Pin();
	if (DebuggerView.IsValid())
	{
		bIsVerbose = DebuggerView->GetViewModel()->IsVerbose();
	}

	UnfilteredDatabaseRows.Reset();
	for (const FTraceMotionMatchingStateDatabaseEntry& DbEntry : State.DatabaseEntries)
	{
		const UPoseSearchDatabase* Database = FTraceMotionMatchingState::GetObjectFromId<UPoseSearchDatabase>(DbEntry.DatabaseId);
		if (FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(Database, ERequestAsyncBuildFlag::ContinueRequest))
		{
			const FPoseSearchIndex& SearchIndex = Database->GetSearchIndex();
			for (const FTraceMotionMatchingStatePoseEntry& PoseEntry : DbEntry.PoseEntries)
			{
				if (const FPoseSearchIndexAsset* SearchIndexAsset = SearchIndex.GetAssetForPoseSafe(PoseEntry.DbPoseIdx))
				{
					TSharedRef<FDebuggerDatabaseRowData>& Row = UnfilteredDatabaseRows.Add_GetRef(MakeShared<FDebuggerDatabaseRowData>());

					const float Time = SearchIndex.GetAssetTime(PoseEntry.DbPoseIdx, Database->Schema->GetSamplingInterval());

					Row->PoseIdx = PoseEntry.DbPoseIdx;
					Row->SourceDatabase = Database;
					Row->DatabaseName = Database->GetName();
					Row->DatabasePath = Database->GetPathName();
					Row->PoseCandidateFlags = PoseEntry.PoseCandidateFlags;
					Row->DbAssetIdx = SearchIndexAsset->SourceAssetIdx;
					Row->AssetTime = Time;
					Row->bMirrored = SearchIndexAsset->bMirrored;
					Row->PoseCost = PoseEntry.Cost;

					Row->CostVector.SetNum(Database->Schema->SchemaCardinality);
					TConstArrayView<float> PoseValues = SearchIndex.GetPoseValuesSafe(PoseEntry.DbPoseIdx);

					// in case we modify the schema while PIE is paused and displaying the Pose Search Editor, we could end up with a stale State with a DbEntry.QueryVector saved with the previous schema
					// so the cardinality of DbEntry.QueryVector and PoseValues don't match. In that case we just use PoseValues as query to have all costs set to zero
					const bool bIsQueryVectorValid = DbEntry.QueryVector.Num() == PoseValues.Num();
					CompareFeatureVectors(PoseValues, bIsQueryVectorValid ? DbEntry.QueryVector : PoseValues, SearchIndex.WeightsSqrt, Row->CostVector);

					const FInstancedStruct& DatabaseAssetStruct = Database->GetAnimationAssetStruct(*SearchIndexAsset);
					if (const FPoseSearchDatabaseAnimationAssetBase* DatabaseAsset = DatabaseAssetStruct.GetPtr<FPoseSearchDatabaseAnimationAssetBase>())
					{
						Row->AssetName = DatabaseAsset->GetName();
						Row->AssetPath = DatabaseAsset->GetAnimationAsset() ? DatabaseAsset->GetAnimationAsset()->GetPathName() : "";
						Row->AssetType = DatabaseAsset->GetSearchIndexType();
						Row->bLooping = DatabaseAsset->IsLooping();
						Row->BlendParameters = FVector::Zero();
						Row->AnimFrame = 0;
						Row->AnimPercentage = 0.0f;

						if (const FPoseSearchDatabaseSequence* DatabaseSequence = DatabaseAssetStruct.GetPtr<FPoseSearchDatabaseSequence>())
						{
							Row->AnimFrame = DatabaseSequence->Sequence->GetFrameAtTime(Time);
							Row->AnimPercentage = Time / DatabaseSequence->Sequence->GetPlayLength();
						}
						else if (const FPoseSearchDatabaseAnimComposite* DatabaseAnimComposite = DatabaseAssetStruct.GetPtr<FPoseSearchDatabaseAnimComposite>())
						{
							Row->AnimFrame = DatabaseAnimComposite->AnimComposite->GetFrameAtTime(Time);
							Row->AnimPercentage = Time / DatabaseAnimComposite->AnimComposite->GetPlayLength();
						}
						else if (const FPoseSearchDatabaseBlendSpace* DatabaseBlendSpace = DatabaseAssetStruct.GetPtr<FPoseSearchDatabaseBlendSpace>())
						{
							Row->BlendParameters = SearchIndexAsset->BlendParameters;
						}
						else
						{
							checkNoEntry();
						}
					}
				}
			}
		}
	}

	SCostBreakDownData CostBreakDownData(State.DatabaseEntries, bIsVerbose);
	if (!UnfilteredDatabaseRows.IsEmpty())
	{
		CostBreakDownData.ProcessData(UnfilteredDatabaseRows);

		// calculating breakdowns min max and colors
		TArray<float> MinCostBreakdowns;
		TArray<float> MaxCostBreakdowns;

		const int32 CostBreakdownsCardinality = UnfilteredDatabaseRows[0]->CostBreakdowns.Num();
		MinCostBreakdowns.Init(UE_MAX_FLT, CostBreakdownsCardinality);
		MaxCostBreakdowns.Init(-UE_MAX_FLT, CostBreakdownsCardinality);

		auto ArrayMinMax = [](TConstArrayView<float> View, TArrayView<float> Min, TArrayView<float> Max, float InvalidValue)
		{
			const int32 Num = View.Num();
			check(Num == Min.Num() && Num == Max.Num());
			for (int i = 0; i < Num; ++i)
			{
				const float Value = View[i];
				if (Value != InvalidValue)
				{
					Min[i] = FMath::Min(Min[i], Value);
					Max[i] = FMath::Max(Max[i], Value);
				}
			}
		};

		for (auto& UnfilteredRow : UnfilteredDatabaseRows)
		{
			ArrayMinMax(UnfilteredRow->CostBreakdowns, MinCostBreakdowns, MaxCostBreakdowns, UE_MAX_FLT);
		}
		
		auto ArraySafeNormalize = [](TConstArrayView<float> View, TConstArrayView<float> Min, TConstArrayView<float> Max, TArrayView<float> NormalizedView)
		{
			const int32 Num = View.Num();
			check(Num == Min.Num() && Num == Max.Num() && Num == NormalizedView.Num());
			for (int i = 0; i < Num; ++i)
			{
				const float Delta = Max[i] - Min[i];
				if (FMath::IsNearlyZero(Delta, UE_KINDA_SMALL_NUMBER))
				{
					NormalizedView[i] = 0.f;
				}
				else
				{
					NormalizedView[i] = (View[i] - Min[i]) / Delta;
				}
			}
		};

		auto LinearColorBlend = [](FLinearColor LinearColorA, FLinearColor LinearColorB, float BlendParam) -> FLinearColor
		{
			return LinearColorA + (LinearColorB - LinearColorA) * BlendParam;
		};

		auto LinearColorArrayBlend = [](FLinearColor LinearColorA, FLinearColor LinearColorB, TConstArrayView<float> BlendParam, TArray<FLinearColor>& BlendedColors) -> void
		{
			const int32 Num = BlendParam.Num();
			BlendedColors.SetNumUninitialized(Num);
			for (int i = 0; i < Num; ++i)
			{
				BlendedColors[i] = LinearColorA + (LinearColorB - LinearColorA) * BlendParam[i];
			}
		};

		TArray<float> CostBreakdownsColorBlend;
		CostBreakdownsColorBlend.Init(0, CostBreakdownsCardinality);
		for (auto& UnfilteredRow : UnfilteredDatabaseRows)
		{
			if (EnumHasAnyFlags(UnfilteredRow->PoseCandidateFlags, EPoseCandidateFlags::AnyValidMask))
			{
				ArraySafeNormalize(UnfilteredRow->CostBreakdowns, MinCostBreakdowns, MaxCostBreakdowns, CostBreakdownsColorBlend);
				LinearColorArrayBlend(BestScoreRowColor, WorstScoreRowColor, CostBreakdownsColorBlend, UnfilteredRow->CostBreakdownsColors);
			}
			else
			{
				UnfilteredRow->CostBreakdownsColors.Init(DiscardedRowColor, CostBreakdownsCardinality);
			}
		}

		float MinCost = UE_MAX_FLT;
		float MaxCost = -UE_MAX_FLT;
		for (auto& UnfilteredRow : UnfilteredDatabaseRows)
		{
			const float Cost = UnfilteredRow->PoseCost.GetTotalCost();
			MinCost = FMath::Min(MinCost, Cost);
			MaxCost = FMath::Max(MaxCost, Cost);
		}

		const float DeltaCost = MaxCost - MinCost;
		for (auto& UnfilteredRow : UnfilteredDatabaseRows)
		{
			if (EnumHasAnyFlags(UnfilteredRow->PoseCandidateFlags, EPoseCandidateFlags::AnyValidMask))
			{
				const float CostColorBlend = DeltaCost > UE_KINDA_SMALL_NUMBER ? (UnfilteredRow->PoseCost.GetTotalCost() - MinCost) / DeltaCost : 0.f;
				UnfilteredRow->CostColor = LinearColorBlend(BestScoreRowColor, WorstScoreRowColor, CostColorBlend);
			}
			else
			{
				UnfilteredRow->CostColor = DiscardedRowColor;
			}
		}
	}

	if (!CostBreakDownData.AreLabelsEqualTo(OldLabels))
	{
		OldLabels = CostBreakDownData.GetLabels();

		// recreating and binding the columns
		Columns.Reset();

		// Construct all column types
		int32 ColumnIdx = 0;
		AddColumn(MakeShared<FDatabaseName>(ColumnIdx++));
		AddColumn(MakeShared<FAssetName>(ColumnIdx++));

		auto CostColumn = MakeShared<FCost>(ColumnIdx++);
		AddColumn(CostColumn);

		int32 LabelIdx = 0;
		for (const FText& Label : CostBreakDownData.GetLabels())
		{
			AddColumn(MakeShared<FChannelBreakdownCostColumn>(ColumnIdx++, LabelIdx++, Label));
		}

#if WITH_EDITORONLY_DATA
		AddColumn(MakeShared<FCostModifier>(ColumnIdx++));
#endif // WITH_EDITORONLY_DATA
		AddColumn(MakeShared<FFrame>(ColumnIdx++));
		AddColumn(MakeShared<FMirrored>(ColumnIdx++));
		AddColumn(MakeShared<FLooping>(ColumnIdx++));
		AddColumn(MakeShared<FPoseIdx>(ColumnIdx++));
		AddColumn(MakeShared<FBlendParameters>(ColumnIdx++));
		AddColumn(MakeShared<FPoseCandidateFlags>(ColumnIdx++));

		SortColumn = CostColumn->ColumnId;

		// Active and Continuing Pose view scroll bars only for indenting the columns to align w/ database
		ActiveView.ScrollBar->SetVisibility(EVisibility::Hidden);
		ContinuingPoseView.ScrollBar->SetVisibility(EVisibility::Hidden);

		// Refresh Columns
		ActiveView.HeaderRow->ClearColumns();
		ContinuingPoseView.HeaderRow->ClearColumns();
		FilteredDatabaseView.HeaderRow->ClearColumns();

		// Sort columns by index
		Columns.ValueSort([](const TSharedRef<IColumn> Column0, const TSharedRef<IColumn> Column1)
		{
			return Column0->SortIndex < Column1->SortIndex;
		});

		// Add columns from map to header row
		for (TPair<FName, TSharedRef<IColumn>>& ColumnPair : Columns)
		{
			IColumn& Column = ColumnPair.Value.Get();
			if (ColumnPair.Value->bEnabled)
			{
				SHeaderRow::FColumn::FArguments ColumnArgs = SHeaderRow::FColumn::FArguments()
					.ColumnId(Column.ColumnId)
					.DefaultLabel(Column.GetLabel())
					.SortMode(this, &SDebuggerDatabaseView::GetColumnSortMode, Column.ColumnId)
					.OnSort(this, &SDebuggerDatabaseView::OnColumnSortModeChanged)
					.FillWidth(this, &SDebuggerDatabaseView::GetColumnWidth, Column.ColumnId)
					.VAlignCell(VAlign_Center)
					.VAlignHeader(VAlign_Center)
					.HAlignHeader(HAlign_Center)
					.HAlignCell(HAlign_Fill);

				FilteredDatabaseView.HeaderRow->AddColumn(ColumnArgs);

				// Every time the active column is changed, update the database column
				ActiveView.HeaderRow->AddColumn(ColumnArgs.OnWidthChanged(this, &SDebuggerDatabaseView::OnColumnWidthChanged, Column.ColumnId));

				ContinuingPoseView.HeaderRow->AddColumn(ColumnArgs.OnWidthChanged(this, &SDebuggerDatabaseView::OnColumnWidthChanged, Column.ColumnId));
			}
		}
	}

	SortDatabaseRows();
	PopulateViewRows();
}

void SDebuggerDatabaseView::AddColumn(TSharedRef<DebuggerDatabaseColumns::IColumn>&& Column)
{
	Columns.Add(Column->ColumnId, Column);
}

EColumnSortMode::Type SDebuggerDatabaseView::GetColumnSortMode(const FName ColumnId) const
{
	if (ColumnId == SortColumn)
	{
		return SortMode;
	}
	
	return EColumnSortMode::None;
}

float SDebuggerDatabaseView::GetColumnWidth(const FName ColumnId) const
{
	check(Columns.Find(ColumnId));

	return Columns[ColumnId]->Width;
}

void SDebuggerDatabaseView::OnColumnSortModeChanged(const EColumnSortPriority::Type SortPriority, const FName & ColumnId, const EColumnSortMode::Type InSortMode)
{
	check(Columns.Find(ColumnId));
	SortColumn = ColumnId;
	SortMode = InSortMode;
	SortDatabaseRows();
	PopulateViewRows();
}

void SDebuggerDatabaseView::OnColumnWidthChanged(const float NewWidth, FName ColumnId) const
{
	check(Columns.Find(ColumnId));
	
	Columns[ColumnId]->Width = NewWidth;
}

void SDebuggerDatabaseView::OnFilterTextChanged(const FText& SearchText)
{
	FilterText = SearchText;
	PopulateViewRows();
}

void SDebuggerDatabaseView::OnHideInvalidPosesCheckboxChanged(ECheckBoxState State)
{
	if (State == ECheckBoxState::Checked) 
	{
		bHideInvalidPoses = true;
	}
	else 
	{
		bHideInvalidPoses = false;
	}
	PopulateViewRows();
}

void SDebuggerDatabaseView::OnDatabaseRowSelectionChanged(TSharedPtr<FDebuggerDatabaseRowData> Row, ESelectInfo::Type SelectInfo)
{
	if (Row.IsValid())
	{
		OnPoseSelectionChanged.ExecuteIfBound(Row->SourceDatabase.Get(), Row->PoseIdx, Row->AssetTime);
	}
}

void SDebuggerDatabaseView::SortDatabaseRows()
{
	if (!UnfilteredDatabaseRows.IsEmpty())
	{
		if (SortMode == EColumnSortMode::Ascending)
		{
			UnfilteredDatabaseRows.Sort(Columns[SortColumn]->GetSortPredicate());
		}
		else if (SortMode == EColumnSortMode::Descending)
		{
			auto DescendingPredicate = [this](const auto& Lhs, const auto& Rhs) -> bool
			{
				return !Columns[SortColumn]->GetSortPredicate()(Lhs, Rhs);
			};

			UnfilteredDatabaseRows.Sort(DescendingPredicate);
		}
	}
}

void SDebuggerDatabaseView::PopulateViewRows()
{
	ActiveView.Rows.Reset();
	ContinuingPoseView.Rows.Reset();
	FilteredDatabaseView.Rows.Empty();

	FString FilterString = FilterText.ToString();
	TArray<FString> Tokens;
	FilterString.ParseIntoArrayWS(Tokens);
	const bool bHasNameFilter = !Tokens.IsEmpty();

	for (const auto& UnfilteredRow : UnfilteredDatabaseRows)
	{
		bool bTryAddToFilteredDatabaseViewRows = true;
		if (EnumHasAnyFlags(UnfilteredRow->PoseCandidateFlags, EPoseCandidateFlags::Valid_ContinuingPose))
		{
			ContinuingPoseView.Rows.Add(UnfilteredRow);
			bTryAddToFilteredDatabaseViewRows = false;
		}

		if (EnumHasAnyFlags(UnfilteredRow->PoseCandidateFlags, EPoseCandidateFlags::Valid_CurrentPose))
		{
			ActiveView.Rows.Add(UnfilteredRow);
			bTryAddToFilteredDatabaseViewRows = false;
		}

		if (bTryAddToFilteredDatabaseViewRows)
		{
			if (!bHideInvalidPoses || EnumHasAnyFlags(UnfilteredRow->PoseCandidateFlags, EPoseCandidateFlags::AnyValidMask))
			{
				bool bPassesNameFilter = true;
				if (bHasNameFilter)
				{
					bPassesNameFilter = Algo::AllOf(Tokens, [&](FString Token)
					{
						return UnfilteredRow->AssetName.Contains(Token);
					});
				}

				if (bPassesNameFilter)
				{
					FilteredDatabaseView.Rows.Add(UnfilteredRow);
				}
			}
		}
	}

	ActiveView.ListView->RequestListRefresh();
	ContinuingPoseView.ListView->RequestListRefresh();
	FilteredDatabaseView.ListView->RequestListRefresh();
}

TSharedRef<ITableRow> SDebuggerDatabaseView::HandleGenerateDatabaseRow(TSharedRef<FDebuggerDatabaseRowData> Item, const TSharedRef<STableViewBase>& OwnerTable) const
{
	return SNew(SDebuggerDatabaseRow, OwnerTable, Item, FilteredDatabaseView.RowStyle, &FilteredDatabaseView.RowBrush, FMargin(0.0f, 2.0f, 6.0f, 2.0f))
		.ColumnMap(this, &SDebuggerDatabaseView::GetColumnMap);	
}

TSharedRef<ITableRow> SDebuggerDatabaseView::HandleGenerateActiveRow(TSharedRef<FDebuggerDatabaseRowData> Item, const TSharedRef<STableViewBase>& OwnerTable) const
{
	return SNew(SDebuggerDatabaseRow, OwnerTable, Item, ActiveView.RowStyle, &ActiveView.RowBrush, FMargin(0.0f, 2.0f, 6.0f, 4.0f))
		.ColumnMap(this, &SDebuggerDatabaseView::GetColumnMap);
}

TSharedRef<ITableRow> SDebuggerDatabaseView::HandleGenerateContinuingPoseRow(TSharedRef<FDebuggerDatabaseRowData> Item, const TSharedRef<STableViewBase>& OwnerTable) const
{
	return SNew(SDebuggerDatabaseRow, OwnerTable, Item, ContinuingPoseView.RowStyle, &ContinuingPoseView.RowBrush, FMargin(0.0f, 2.0f, 6.0f, 4.0f))
		.ColumnMap(this, &SDebuggerDatabaseView::GetColumnMap);
}

void SDebuggerDatabaseView::Construct(const FArguments& InArgs)
{
	ParentDebuggerViewPtr = InArgs._Parent;
	OnPoseSelectionChanged = InArgs._OnPoseSelectionChanged;
	check(OnPoseSelectionChanged.IsBound());

	// Active Row
	ActiveView.HeaderRow = SNew(SHeaderRow);

	// Used for spacing
	ActiveView.ScrollBar = SNew(SScrollBar)
		.Orientation(Orient_Vertical)
		.HideWhenNotInUse(false)
		.AlwaysShowScrollbar(true)
		.AlwaysShowScrollbarTrack(true);

	ActiveView.ListView = SNew(SListView<TSharedRef<FDebuggerDatabaseRowData>>)
		.ListItemsSource(&ActiveView.Rows)
		.HeaderRow(ActiveView.HeaderRow.ToSharedRef())
		.OnGenerateRow(this, &SDebuggerDatabaseView::HandleGenerateActiveRow)
		.ExternalScrollbar(ActiveView.ScrollBar)
		.SelectionMode(ESelectionMode::SingleToggle)
		.ConsumeMouseWheel(EConsumeMouseWheel::Never);

	ActiveView.RowStyle = FAppStyle::GetWidgetStyle<FTableRowStyle>("TableView.Row");
	ActiveView.RowBrush = *FAppStyle::GetBrush("DetailsView.CategoryTop");

	// ContinuingPose Row
	ContinuingPoseView.HeaderRow = SNew(SHeaderRow).Visibility(EVisibility::Collapsed);

	// Used for spacing
	ContinuingPoseView.ScrollBar = SNew(SScrollBar)
		.Orientation(Orient_Vertical)
		.HideWhenNotInUse(false)
		.AlwaysShowScrollbar(true)
		.AlwaysShowScrollbarTrack(true);

	ContinuingPoseView.ListView = SNew(SListView<TSharedRef<FDebuggerDatabaseRowData>>)
		.ListItemsSource(&ContinuingPoseView.Rows)
		.HeaderRow(ContinuingPoseView.HeaderRow.ToSharedRef())
		.OnGenerateRow(this, &SDebuggerDatabaseView::HandleGenerateContinuingPoseRow)
		.ExternalScrollbar(ContinuingPoseView.ScrollBar)
		.SelectionMode(ESelectionMode::SingleToggle)
		.ConsumeMouseWheel(EConsumeMouseWheel::Never);

	ContinuingPoseView.RowStyle = FAppStyle::GetWidgetStyle<FTableRowStyle>("TableView.Row");
	ContinuingPoseView.RowBrush = *FAppStyle::GetBrush("DetailsView.CategoryTop");

	// Filtered Database
	FilteredDatabaseView.ScrollBar =
		SNew(SScrollBar)
		.Orientation(Orient_Vertical)
		.HideWhenNotInUse(false)
		.AlwaysShowScrollbar(true)
		.AlwaysShowScrollbarTrack(true);
	FilteredDatabaseView.HeaderRow = SNew(SHeaderRow).Visibility(EVisibility::Collapsed);

	FilteredDatabaseView.ListView = SNew(SListView<TSharedRef<FDebuggerDatabaseRowData>>)
		.ListItemsSource(&FilteredDatabaseView.Rows)
		.HeaderRow(FilteredDatabaseView.HeaderRow.ToSharedRef())
		.OnGenerateRow(this, &SDebuggerDatabaseView::HandleGenerateDatabaseRow)
		.ExternalScrollbar(FilteredDatabaseView.ScrollBar)
		.SelectionMode(ESelectionMode::Multi)
		.ConsumeMouseWheel(EConsumeMouseWheel::WhenScrollingPossible)
		.OnSelectionChanged(this, &SDebuggerDatabaseView::OnDatabaseRowSelectionChanged);

	FilteredDatabaseView.RowStyle = FAppStyle::GetWidgetStyle<FTableRowStyle>("TableView.Row");
	// Set selected color to white to retain visibility when multi-selecting
	FilteredDatabaseView.RowStyle.SetSelectedTextColor(FLinearColor(FVector3f(0.8f)));
	FilteredDatabaseView.RowBrush = *FAppStyle::GetBrush("ToolPanel.GroupBorder");

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		// Side and top margins, ignore bottom handled by the color border below
		.Padding(0.0f, 5.0f, 0.0f, 0.0f)
		.AutoHeight()
		[
			// Active Row text tab
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Fill)
				.Padding(0.0f)
				.AutoWidth()
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::GetBrush("DetailsView.CategoryTop"))
					.Padding(FMargin(30.0f, 3.0f, 30.0f, 0.0f))
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Fill)
					[
						SNew(STextBlock)
						.Text(FText::FromString("Active Pose"))	
					]
				]
			]

			// Active row list view with scroll bar
			+ SVerticalBox::Slot()
			
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				
				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Fill)
				.Padding(0.0f)
				[
					SNew(SBorder)
					
					.BorderImage(FAppStyle::GetBrush("NoBorder"))
					.Padding(0.0f)
					[
						ActiveView.ListView.ToSharedRef()
					]
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					ActiveView.ScrollBar.ToSharedRef()
				]
			]	
		]

		+ SVerticalBox::Slot()
		// Side and top margins, ignore bottom handled by the color border below
		.Padding(0.0f, 5.0f, 0.0f, 0.0f)
		.AutoHeight()
		[
			// ContinuingPose Row text tab
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Fill)
				.Padding(0.0f)
				.AutoWidth()
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::GetBrush("DetailsView.CategoryTop"))
					.Padding(FMargin(30.0f, 3.0f, 30.0f, 0.0f))
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Fill)
					[
						SNew(STextBlock)
						.Text(FText::FromString("Continuing Pose"))	
					]
				]
			]

			// ContinuingPose row list view with scroll bar
			+ SVerticalBox::Slot()
			
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				
				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Fill)
				.Padding(0.0f)
				[
					SNew(SBorder)
					
					.BorderImage(FAppStyle::GetBrush("NoBorder"))
					.Padding(0.0f)
					[
						ContinuingPoseView.ListView.ToSharedRef()
					]
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					ContinuingPoseView.ScrollBar.ToSharedRef()
				]
			]	
		]
		
		+ SVerticalBox::Slot()
		.Padding(0.0f, 0.0f, 0.0f, 5.0f)
		[
			// Database view text tab
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Fill)
				.Padding(0.0f)
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::GetBrush("DetailsView.CategoryTop"))
					.Padding(FMargin(30.0f, 3.0f, 30.0f, 0.0f))
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Fill)
					[
						SNew(STextBlock)
						.Text(FText::FromString("Pose Candidates"))
					]
				]
				.AutoWidth()
				
				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Fill)
				[
					SNew(SBorder)
					.BorderImage(&FilteredDatabaseView.RowStyle.EvenRowBackgroundBrush)
				]
			]
			.AutoHeight()

			// Gray line below the tab 
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("DetailsView.CategoryTop"))
				.Padding(FMargin(0.0f, 3.0f, 0.0f, 3.0f))
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Fill)
			]

			+ SVerticalBox::Slot()
			.Padding(0.0f, 0.0f, 0.0f, 5.0f)
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.Padding(10, 5, 10, 5)
				[
					SAssignNew(FilterBox, SSearchBox)
					.OnTextChanged(this, &SDebuggerDatabaseView::OnFilterTextChanged)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(10, 5, 10, 5)
				[
					SNew(SCheckBox)
					.OnCheckStateChanged_Lambda([this](ECheckBoxState State)
					{
						   SDebuggerDatabaseView::OnHideInvalidPosesCheckboxChanged(State);
					})
					[
						SNew(STextBlock)
						.Text(LOCTEXT("PoseSearchDebuggerHideInvalidPosesFlag", "Hide Invalid Poses"))
					]
				]
			]
		
			+ SVerticalBox::Slot()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.Padding(0.0f)
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::GetBrush("NoBorder"))
					.Padding(0.0f)
					[
						FilteredDatabaseView.ListView.ToSharedRef()
					]
				]
				
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					FilteredDatabaseView.ScrollBar.ToSharedRef()
				]
			]
		]
	];

	SortMode = EColumnSortMode::Ascending;
	OldLabels.Reset();
	Columns.Reset();
}

} // namespace UE::PoseSearch

#undef LOCTEXT_NAMESPACE
