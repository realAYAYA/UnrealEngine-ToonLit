// Copyright Epic Games, Inc. All Rights Reserved.

#include "SMemAllocTableTreeView.h"

#include "Containers/Set.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "GenericPlatform/GenericPlatformMisc.h"
#include "ISourceCodeAccessModule.h"
#include "ISourceCodeAccessor.h"
#include "Modules/ModuleManager.h"
#include "SlateOptMacros.h"
#include "Styling/AppStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/SToolTip.h"

// TraceServices
#include "Common/ProviderLock.h"
#include "TraceServices/Model/AllocationsProvider.h"
#include "TraceServices/Model/Callstack.h"
#include "TraceServices/Model/Definitions.h"
#include "TraceServices/Model/MetadataProvider.h"
#include "TraceServices/Model/Modules.h"
#include "TraceServices/Model/Strings.h"

// Insights
#include "Insights/InsightsStyle.h"
#include "Insights/MemoryProfiler/Common/SymbolSearchPathsHelper.h"
#include "Insights/MemoryProfiler/MemoryProfilerManager.h"
#include "Insights/MemoryProfiler/ViewModels/MemAllocGroupingByCallstack.h"
#include "Insights/MemoryProfiler/ViewModels/MemAllocGroupingByHeap.h"
#include "Insights/MemoryProfiler/ViewModels/MemAllocGroupingBySize.h"
#include "Insights/MemoryProfiler/ViewModels/MemAllocGroupingByTag.h"
#include "Insights/MemoryProfiler/ViewModels/MemAllocNode.h"
#include "Insights/MemoryProfiler/ViewModels/MemAllocTable.h"
#include "Insights/MemoryProfiler/ViewModels/MemorySharedState.h"
#include "Insights/MemoryProfiler/Widgets/SMemoryProfilerWindow.h"
#include "Insights/Table/ViewModels/TableCellValueFormatter.h"
#include "Insights/Table/ViewModels/TableColumn.h"
#include "Insights/TimingProfilerCommon.h"
#include "Insights/ViewModels/FilterConfigurator.h"

#include <limits>

#define LOCTEXT_NAMESPACE "SMemAllocTableTreeView"

namespace Insights
{

const int SMemAllocTableTreeView::FullCallStackIndex = 0x0000FFFFF;
const int SMemAllocTableTreeView::LLMFilterIndex = 0x0000FFFFE;

////////////////////////////////////////////////////////////////////////////////////////////////////

SMemAllocTableTreeView::SMemAllocTableTreeView()
{
	bRunInAsyncMode = true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

SMemAllocTableTreeView::~SMemAllocTableTreeView()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemAllocTableTreeView::Construct(const FArguments& InArgs, TSharedPtr<Insights::FMemAllocTable> InTablePtr)
{
	ConstructWidget(InTablePtr);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemAllocTableTreeView::Reset()
{
	//...
	STableTreeView::Reset();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemAllocTableTreeView::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	STableTreeView::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	if (bHasPendingQueryReset && !bIsUpdateRunning)
	{
		ResetAndStartQuery();
		bHasPendingQueryReset = false;
	}

	if (!bIsUpdateRunning)
	{
		RebuildTree(false);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemAllocTableTreeView::RebuildTree(bool bResync)
{
	FStopwatch Stopwatch;
	Stopwatch.Start();

	FStopwatch SyncStopwatch;
	SyncStopwatch.Start();

	if (bResync)
	{
		TableTreeNodes.Empty();
	}

	const int32 PreviousNodeCount = TableTreeNodes.Num();

	TSharedPtr<Insights::FMemAllocTable> MemAllocTable = GetMemAllocTable();

	if (Session.IsValid() && MemAllocTable.IsValid())
	{
		TraceServices::IAllocationsProvider::EQueryStatus QueryStatus;
		UpdateQuery(QueryStatus);

		if (QueryStatus == TraceServices::IAllocationsProvider::EQueryStatus::Done)
		{
			UE_LOG(MemoryProfiler, Log, TEXT("[MemAlloc] Rebuilding tree..."));
			TArray<FMemoryAlloc>& Allocs = MemAllocTable->GetAllocs();

			const int32 TotalAllocCount = Allocs.Num();
			if (TotalAllocCount != TableTreeNodes.Num())
			{
				UE_LOG(MemoryProfiler, Log, TEXT("[MemAlloc] Creating nodes (%d nodes --> %d allocs)..."), TableTreeNodes.Num(), TotalAllocCount);

				if (TableTreeNodes.Num() > TotalAllocCount)
				{
					TableTreeNodes.Empty();
				}
				TableTreeNodes.Reserve(TotalAllocCount);

				uint32 HeapAllocCount(0);
				const FName BaseNodeName(TEXT("alloc"));
				const FName BaseHeapName(TEXT("heap"));
				for (int32 AllocIndex = TableTreeNodes.Num(); AllocIndex < TotalAllocCount; ++AllocIndex)
				{
					const FMemoryAlloc* Alloc = MemAllocTable->GetMemAlloc(AllocIndex);

					// Until we have an UX story around heap allocations
					// remove them from the list
					if (Alloc->bIsBlock)
					{
						++HeapAllocCount;
						continue;
					}

					FName NodeName(Alloc->bIsBlock ? BaseHeapName : BaseNodeName, static_cast<int32>(Alloc->GetStartEventIndex() + 1));
					FMemAllocNodePtr NodePtr = MakeShared<FMemAllocNode>(NodeName, MemAllocTable, AllocIndex);
					TableTreeNodes.Add(NodePtr);
				}
				ensure(TableTreeNodes.Num() == TotalAllocCount - HeapAllocCount);
				UpdateQueryInfo();
			}
		}
	}

	SyncStopwatch.Stop();

	if (bResync || TableTreeNodes.Num() != PreviousNodeCount)
	{
		// Save selection.
		TArray<FTableTreeNodePtr> SelectedItems;
		TreeView->GetSelectedItems(SelectedItems);

		UpdateTree();

		TreeView->RebuildList();

		// Restore selection.
		if (SelectedItems.Num() > 0)
		{
			TreeView->ClearSelection();
			for (FTableTreeNodePtr& NodePtr : SelectedItems)
			{
				NodePtr = GetNodeByTableRowIndex(NodePtr->GetRowIndex());
			}
			SelectedItems.RemoveAll([](const FTableTreeNodePtr& NodePtr) { return !NodePtr.IsValid(); });
			if (SelectedItems.Num() > 0)
			{
				TreeView->SetItemSelection(SelectedItems, true);
				TreeView->RequestScrollIntoView(SelectedItems.Last());
			}
		}
	}

	Stopwatch.Stop();
	const double TotalTime = Stopwatch.GetAccumulatedTime();
	if (TotalTime > 0.01)
	{
		const double SyncTime = SyncStopwatch.GetAccumulatedTime();
		UE_LOG(MemoryProfiler, Log, TEXT("[MemAlloc] Tree view rebuilt in %.4fs (sync: %.4fs + update: %.4fs) --> %d nodes (%d added)"),
			TotalTime, SyncTime, TotalTime - SyncTime, TableTreeNodes.Num(), TableTreeNodes.Num() - PreviousNodeCount);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemAllocTableTreeView::OnQueryInvalidated()
{
	CancelQuery();

	if (bIsUpdateRunning)
	{
		bHasPendingQueryReset = true;
	}
	else
	{
		ResetAndStartQuery();
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemAllocTableTreeView::ResetAndStartQuery()
{
	TableTreeNodes.Reset();

	TSharedPtr<Insights::FMemAllocTable> MemAllocTable = GetMemAllocTable();
	if (MemAllocTable)
	{
		TArray<FMemoryAlloc>& Allocs = MemAllocTable->GetAllocs();
		Allocs.Reset(10 * 1024 * 1024);
	}

	UpdateQueryInfo();

	StartQuery();

	RebuildTree(true);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemAllocTableTreeView::StartQuery()
{
	check(Query == 0);

	if (!Rule)
	{
		UE_LOG(MemoryProfiler, Warning, TEXT("[MemAlloc] Invalid query rule!"));
		return;
	}

	if (!Session.IsValid())
	{
		UE_LOG(MemoryProfiler, Warning, TEXT("[MemAlloc] Invalid analysis session!"));
		return;
	}

	const TraceServices::IAllocationsProvider* AllocationsProvider = TraceServices::ReadAllocationsProvider(*Session.Get());
	if (!AllocationsProvider)
	{
		UE_LOG(MemoryProfiler, Warning, TEXT("[MemAlloc] Invalid allocations provider!"));
		return;
	}

	{
		const TraceServices::IAllocationsProvider& Provider = *AllocationsProvider;
		TraceServices::FProviderReadScopeLock _(Provider);
		TraceServices::IAllocationsProvider::FQueryParams Params = { Rule->GetValue(), TimeMarkers[0], TimeMarkers[1], TimeMarkers[2], TimeMarkers[3] };
		Query = Provider.StartQuery(Params);
	}

	if (Query == 0)
	{
		UE_LOG(MemoryProfiler, Error, TEXT("[MemAlloc] Unsupported query rule (%s)!"), *Rule->GetShortName().ToString());
	}
	else
	{
		QueryStopwatch.Reset();
		QueryStopwatch.Start();
	}

	//TODO: update window title
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemAllocTableTreeView::UpdateQuery(TraceServices::IAllocationsProvider::EQueryStatus& OutStatus)
{
	if (Query == 0)
	{
		OutStatus = TraceServices::IAllocationsProvider::EQueryStatus::Unknown;
		return;
	}

	if (!Session.IsValid())
	{
		UE_LOG(MemoryProfiler, Warning, TEXT("[MemAlloc] Invalid analysis session!"));
		return;
	}

	const TraceServices::IAllocationsProvider* AllocationsProvider = TraceServices::ReadAllocationsProvider(*Session.Get());
	if (!AllocationsProvider)
	{
		UE_LOG(MemoryProfiler, Warning, TEXT("[MemAlloc] Invalid allocations provider!"));
		return;
	}
	const TraceServices::IAllocationsProvider& Provider = *AllocationsProvider;

	const TraceServices::ICallstacksProvider* CallstacksProvider = TraceServices::ReadCallstacksProvider(*Session.Get());

	const TraceServices::IMetadataProvider* MetadataProvider = TraceServices::ReadMetadataProvider(*Session.Get());

	uint16 AssetMetadataType = 0;
	const TraceServices::FMetadataSchema* Schema = nullptr;
	if (MetadataProvider)
	{
		TraceServices::FProviderReadScopeLock MetadataProviderReadLock(*MetadataProvider);
		AssetMetadataType = MetadataProvider->GetRegisteredMetadataType(TEXT("Asset"));
		if (AssetMetadataType == TraceServices::IMetadataProvider::InvalidMetadataId)
		{
			// If AssetMetadataType is not valid then we do not need to further check the Asset metadata for each allocation.
			MetadataProvider = nullptr;
		}
		else
		{
			Schema = MetadataProvider->GetRegisteredMetadataSchema(AssetMetadataType);
		}
	}

	const TraceServices::IDefinitionProvider* DefinitionProvider = TraceServices::ReadDefinitionProvider(*Session.Get());

	constexpr double MaxPollTime = 0.03; // Stop getting results after 30 ms so we don't tank the frame rate too much.
	FStopwatch TotalStopwatch;
	TotalStopwatch.Start();

	do
	{
		TraceServices::IAllocationsProvider::FQueryStatus Status = Provider.PollQuery(Query);
		OutStatus = Status.Status;

		if (Status.Status <= TraceServices::IAllocationsProvider::EQueryStatus::Done)
		{
			UE_LOG(MemoryProfiler, Log, TEXT("[MemAlloc] Query completed."));
			Query = 0;
			QueryStopwatch.Stop();
			return;
		}

		if (Status.Status == TraceServices::IAllocationsProvider::EQueryStatus::Working)
		{
			break;
		}

		check(Status.Status == TraceServices::IAllocationsProvider::EQueryStatus::Available);

		TSharedPtr<Insights::FMemAllocTable> MemAllocTable = GetMemAllocTable();
		if (MemAllocTable)
		{
			TraceServices::FProviderReadScopeLock _(Provider);

			TArray<FMemoryAlloc>& Allocs = MemAllocTable->GetAllocs();

			FStopwatch ResultStopwatch;
			FStopwatch PageStopwatch;
			ResultStopwatch.Start();
			uint32 PageCount = 0;
			uint32 TotalAllocCount = 0;

			// Multiple 'pages' of results will be returned. No guarantees are made
			// about the order of pages or the allocations they report.
			TraceServices::IAllocationsProvider::FQueryResult Result = Status.NextResult();
			while (Result.IsValid())
			{
				UE_LOG(MemoryProfiler, Log, TEXT("[MemAlloc] Page with %u allocs..."), Result->Num());

				++PageCount;
				PageStopwatch.Restart();

				const uint32 AllocCount = Result->Num();
				TotalAllocCount += AllocCount;

				int32 AllocsDestIndex = Allocs.Num();
				Allocs.AddUninitialized(AllocCount);

				for (uint32 AllocIndex = 0; AllocIndex < AllocCount; ++AllocIndex, ++AllocsDestIndex)
				{
					const TraceServices::IAllocationsProvider::FAllocation* Allocation = Result->Get(AllocIndex);
					FMemoryAlloc& Alloc = Allocs[AllocsDestIndex];

					Alloc.StartEventIndex = Allocation->GetStartEventIndex();
					Alloc.EndEventIndex = Allocation->GetEndEventIndex();

					Alloc.StartTime = Allocation->GetStartTime();
					Alloc.EndTime = Allocation->GetEndTime();

					Alloc.Address = Allocation->GetAddress();
					Alloc.Size = int64(Allocation->GetSize());

					Alloc.TagId = Allocation->GetTag();
					Alloc.Tag = Provider.GetTagFullPath(Allocation->GetTag());

					Alloc.Asset = nullptr;
					Alloc.ClassName = nullptr;
					const uint32 MetadataId = Allocation->GetMetadataId();
					if (MetadataId != TraceServices::IMetadataProvider::InvalidMetadataId && MetadataProvider && DefinitionProvider)
					{
						TraceServices::FProviderReadScopeLock MetadataProviderReadLock(*MetadataProvider);
						MetadataProvider->EnumerateMetadata(Allocation->GetThreadId(), MetadataId,
							[AssetMetadataType, Schema, &Alloc, DefinitionProvider](uint32 StackDepth, uint16 Type, const void* Data, uint32 Size) -> bool
							{
								if (Type == AssetMetadataType)
								{
									TraceServices::IDefinitionProvider::FReadScopeLock DefinitionProviderReadLock(*DefinitionProvider);
									const auto Reader = Schema->Reader();
									const auto AssetNameRef = Reader.GetValueAs<UE::Trace::FEventRef32>((uint8*)Data, 0);
									const auto AssetName = DefinitionProvider->Get<TraceServices::FStringDefinition>(*AssetNameRef);
									if (AssetName)
									{
										Alloc.Asset = AssetName->Display;
									}
									const auto ClassNameRef = Reader.GetValueAs<UE::Trace::FEventRef32>((uint8*)Data, 1);
									const auto ClassName = DefinitionProvider->Get<TraceServices::FStringDefinition>(*ClassNameRef);
									if (ClassName)
									{
										Alloc.ClassName = ClassName->Display;
									}
									return false;
								}
								return true;
							});
					}

					if (CallstacksProvider)
					{
						Alloc.Callstack = CallstacksProvider->GetCallstack(Allocation->GetCallstackId());
						check(Alloc.Callstack != nullptr);

						Alloc.FreeCallstack = CallstacksProvider->GetCallstack(Allocation->GetFreeCallstackId());
						check(Alloc.FreeCallstack != nullptr);
					}
					else
					{
						Alloc.Callstack = nullptr;
						Alloc.FreeCallstack = nullptr;
					}

					Alloc.RootHeap = Allocation->GetRootHeap();
					Alloc.bIsBlock = Allocation->IsHeap();

					Alloc.bIsDecline = false;
					if (Rule->GetValue() == TraceServices::IAllocationsProvider::EQueryRule::aAfaBf)
					{
						if (Alloc.StartTime <= TimeMarkers[0] && Alloc.EndTime <= TimeMarkers[1]) // decline
						{
							Alloc.Size = -Alloc.Size;
							Alloc.bIsDecline = true;
						}
					}
				}

				PageStopwatch.Stop();
				const double PageTime = PageStopwatch.GetAccumulatedTime();
				if (PageTime > 0.01)
				{
					const double Speed = (PageTime * 1000000.0) / AllocCount;
					UE_LOG(MemoryProfiler, Log, TEXT("[MemAlloc] Query result for page %u (%u allocs, slack=%u) retrieved in %.3fs (speed: %.3f seconds per 1M allocs)."), PageCount, AllocCount, Allocs.GetSlack(), PageTime, Speed);
				}

				Result = Status.NextResult();
			}

			ResultStopwatch.Stop();
			const double TotalTime = ResultStopwatch.GetAccumulatedTime();
			if (TotalTime > 0.01)
			{
				const double Speed = (TotalTime * 1000000.0) / TotalAllocCount;
				UE_LOG(MemoryProfiler, Log, TEXT("[MemAlloc] Query results (%u pages, %u allocs, slack=%u) retrieved in %.3fs (speed: %.3f seconds per 1M allocs)."), PageCount, TotalAllocCount, Allocs.GetSlack(), TotalTime, Speed);
			}
		}

		TotalStopwatch.Update();
	}
	while (OutStatus == TraceServices::IAllocationsProvider::EQueryStatus::Available && TotalStopwatch.GetAccumulatedTime() < MaxPollTime);

	TotalStopwatch.Stop();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemAllocTableTreeView::CancelQuery()
{
	if (Query != 0)
	{
		if (Session.IsValid())
		{
			const TraceServices::IAllocationsProvider* AllocationsProvider = TraceServices::ReadAllocationsProvider(*Session.Get());
			if (AllocationsProvider)
			{
				AllocationsProvider->CancelQuery(Query);
				UE_LOG(MemoryProfiler, Log, TEXT("[MemAlloc] Query canceled."));
			}
		}

		Query = 0;
		QueryStopwatch.Stop();
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool SMemAllocTableTreeView::IsRunning() const
{
	return Query != 0 || STableTreeView::IsRunning();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

double SMemAllocTableTreeView::GetAllOperationsDuration()
{
	if (Query != 0)
	{
		QueryStopwatch.Update();
		return QueryStopwatch.GetAccumulatedTime();
	}

	return STableTreeView::GetAllOperationsDuration();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FText SMemAllocTableTreeView::GetCurrentOperationName() const
{
	if (Query != 0)
	{
		return LOCTEXT("CurrentOperationName", "Running Query");
	}

	return STableTreeView::GetCurrentOperationName();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
TSharedPtr<SWidget> SMemAllocTableTreeView::ConstructToolbar()
{
	TSharedPtr<SHorizontalBox> Box = SNew(SHorizontalBox);

	Box->AddSlot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(0.0f, 0.0f, 4.0f, 0.0f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("Preset", "Preset:"))
		];

	Box->AddSlot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(SBox)
			.MinDesiredWidth(150.0f)
			[
				SAssignNew(PresetComboBox, SComboBox<TSharedRef<ITableTreeViewPreset>>)
				.ToolTipText(this, &SMemAllocTableTreeView::ViewPreset_GetSelectedToolTipText)
				.OptionsSource(GetAvailableViewPresets())
				.OnSelectionChanged(this, &SMemAllocTableTreeView::ViewPreset_OnSelectionChanged)
				.OnGenerateWidget(this, &SMemAllocTableTreeView::ViewPreset_OnGenerateWidget)
				[
					SNew(STextBlock)
					.Text(this, &SMemAllocTableTreeView::ViewPreset_GetSelectedText)
				]
			]
		];

	//for (const TSharedRef<ITableTreeViewPreset>& ViewPreset : AvailableViewPresets)
	//{
	//	Box->AddSlot()
	//		.AutoWidth()
	//		.Padding(4.0f, 0.0f, 0.0f, 0.0f)
	//		[
	//			SNew(SButton)
	//			.Text(ViewPreset->GetName())
	//			.ToolTipText(ViewPreset->GetToolTip())
	//			.OnClicked(this, &SMemAllocTableTreeView::OnApplyViewPreset, (const ITableTreeViewPreset*)&ViewPreset.Get())
	//		];
	//}

	Box->AddSlot()
		.AutoWidth()
		.Padding(4.0f, 0.0f, 0.0f, 0.0f)
		[
			ConstructFunctionToggleButton()
		];

	return Box;
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemAllocTableTreeView::InitAvailableViewPresets()
{
	//////////////////////////////////////////////////
	// Default View

	class FDefaultViewPreset : public ITableTreeViewPreset
	{
	public:
		virtual FText GetName() const override
		{
			return LOCTEXT("Default_PresetName", "Default");
		}
		virtual FText GetToolTip() const override
		{
			return LOCTEXT("Default_PresetToolTip", "Default View\nConfigure the tree view to show default allocation info.");
		}
		virtual FName GetSortColumn() const override
		{
			return FTable::GetHierarchyColumnId();
		}
		virtual EColumnSortMode::Type GetSortMode() const override
		{
			return EColumnSortMode::Type::Ascending;
		}
		virtual void SetCurrentGroupings(const TArray<TSharedPtr<FTreeNodeGrouping>>& InAvailableGroupings, TArray<TSharedPtr<FTreeNodeGrouping>>& InOutCurrentGroupings) const override
		{
			InOutCurrentGroupings.Reset();

			check(InAvailableGroupings[0]->Is<FTreeNodeGroupingFlat>());
			InOutCurrentGroupings.Add(InAvailableGroupings[0]);
		}
		virtual void GetColumnConfigSet(TArray<FTableColumnConfig>& InOutConfigSet) const override
		{
			InOutConfigSet.Add({ FTable::GetHierarchyColumnId(),               true, 200.0f });
			InOutConfigSet.Add({ FMemAllocTableColumns::CountColumnId,         true, 100.0f });
			InOutConfigSet.Add({ FMemAllocTableColumns::SizeColumnId,          true, 100.0f });
			InOutConfigSet.Add({ FMemAllocTableColumns::TagColumnId,           true, 120.0f });
			InOutConfigSet.Add({ FMemAllocTableColumns::FunctionColumnId,      true, 550.0f });
			InOutConfigSet.Add({ FMemAllocTableColumns::CallstackSizeColumnId, true, 100.0f });
		}
	};
	AvailableViewPresets.Add(MakeShared<FDefaultViewPreset>());

	//////////////////////////////////////////////////
	// Detailed View

	class FDetailedViewPreset : public ITableTreeViewPreset
	{
	public:
		virtual FText GetName() const override
		{
			return LOCTEXT("Detailed_PresetName", "Detailed");
		}
		virtual FText GetToolTip() const override
		{
			return LOCTEXT("Detailed_PresetToolTip", "Detailed View\nConfigure the tree view to show detailed allocation info.");
		}
		virtual FName GetSortColumn() const override
		{
			return FTable::GetHierarchyColumnId();
		}
		virtual EColumnSortMode::Type GetSortMode() const override
		{
			return EColumnSortMode::Type::Ascending;
		}
		virtual void SetCurrentGroupings(const TArray<TSharedPtr<FTreeNodeGrouping>>& InAvailableGroupings, TArray<TSharedPtr<FTreeNodeGrouping>>& InOutCurrentGroupings) const override
		{
			InOutCurrentGroupings.Reset();

			check(InAvailableGroupings[0]->Is<FTreeNodeGroupingFlat>());
			InOutCurrentGroupings.Add(InAvailableGroupings[0]);
		}
		virtual void GetColumnConfigSet(TArray<FTableColumnConfig>& InOutConfigSet) const override
		{
			InOutConfigSet.Add({ FTable::GetHierarchyColumnId(),                 true, 200.0f });
			InOutConfigSet.Add({ FMemAllocTableColumns::StartEventIndexColumnId, true, 100.0f });
			InOutConfigSet.Add({ FMemAllocTableColumns::EndEventIndexColumnId,   true, 100.0f });
			InOutConfigSet.Add({ FMemAllocTableColumns::EventDistanceColumnId,   true, 100.0f });
			InOutConfigSet.Add({ FMemAllocTableColumns::StartTimeColumnId,       true, 100.0f });
			InOutConfigSet.Add({ FMemAllocTableColumns::EndTimeColumnId,         true, 100.0f });
			InOutConfigSet.Add({ FMemAllocTableColumns::DurationColumnId,        true, 100.0f });
			InOutConfigSet.Add({ FMemAllocTableColumns::AddressColumnId,         true, 120.0f });
			InOutConfigSet.Add({ FMemAllocTableColumns::MemoryPageColumnId,      true, 120.0f });
			InOutConfigSet.Add({ FMemAllocTableColumns::CountColumnId,           true, 100.0f });
			InOutConfigSet.Add({ FMemAllocTableColumns::SizeColumnId,            true, 100.0f });
			InOutConfigSet.Add({ FMemAllocTableColumns::TagColumnId,             true, 120.0f });
			InOutConfigSet.Add({ FMemAllocTableColumns::FunctionColumnId,        true, 550.0f });
			InOutConfigSet.Add({ FMemAllocTableColumns::SourceFileColumnId,      true, 550.0f });
			InOutConfigSet.Add({ FMemAllocTableColumns::CallstackSizeColumnId,   true, 100.0f });
		}
	};
	AvailableViewPresets.Add(MakeShared<FDetailedViewPreset>());

	//////////////////////////////////////////////////
	// Heap Breakdown View

	class FHeapViewPreset : public ITableTreeViewPreset
	{
	public:
		virtual FText GetName() const override
		{
			return LOCTEXT("Heap_PresetName", "Heap");
		}
		virtual FText GetToolTip() const override
		{
			return LOCTEXT("Heap_PresetToolTip", "Heap Breakdown View\nConfigure the tree view to show a breakdown of allocations by their parent heap type.");
		}
		virtual FName GetSortColumn() const override
		{
			return FMemAllocTableColumns::SizeColumnId;
		}
		virtual EColumnSortMode::Type GetSortMode() const override
		{
			return EColumnSortMode::Type::Descending;
		}
		virtual void SetCurrentGroupings(const TArray<TSharedPtr<FTreeNodeGrouping>>& InAvailableGroupings, TArray<TSharedPtr<FTreeNodeGrouping>>& InOutCurrentGroupings) const override
		{
			InOutCurrentGroupings.Reset();

			//check(InAvailableGroupings[0]->Is<FTreeNodeGroupingFlat>());
			//InOutCurrentGroupings.Add(InAvailableGroupings[0]);

			const TSharedPtr<FTreeNodeGrouping>* HeapGrouping = InAvailableGroupings.FindByPredicate(
				[](TSharedPtr<FTreeNodeGrouping>& Grouping)
				{
					return Grouping->Is<FMemAllocGroupingByHeap>();
				});
			if (HeapGrouping)
			{
				InOutCurrentGroupings.Add(*HeapGrouping);
			}
		}
		virtual void GetColumnConfigSet(TArray<FTableColumnConfig>& InOutConfigSet) const override
		{
			InOutConfigSet.Add({ FTable::GetHierarchyColumnId(),          true, 400.0f });
			InOutConfigSet.Add({ FMemAllocTableColumns::CountColumnId,    true, 100.0f });
			InOutConfigSet.Add({ FMemAllocTableColumns::SizeColumnId,     true, 100.0f });
			InOutConfigSet.Add({ FMemAllocTableColumns::TagColumnId,      true, 200.0f });
			InOutConfigSet.Add({ FMemAllocTableColumns::FunctionColumnId, true, 200.0f });
		}
	};
	AvailableViewPresets.Add(MakeShared<FHeapViewPreset>());

	//////////////////////////////////////////////////
	// Size Breakdown View

	class FSizeViewPreset : public ITableTreeViewPreset
	{
	public:
		virtual FText GetName() const override
		{
			return LOCTEXT("Size_PresetName", "Size");
		}
		virtual FText GetToolTip() const override
		{
			return LOCTEXT("Size_PresetToolTip", "Size Breakdown View\nConfigure the tree view to show a breakdown of allocations by their size.");
		}
		virtual FName GetSortColumn() const override
		{
			return FMemAllocTableColumns::SizeColumnId;
		}
		virtual EColumnSortMode::Type GetSortMode() const override
		{
			return EColumnSortMode::Type::Descending;
		}
		virtual void SetCurrentGroupings(const TArray<TSharedPtr<FTreeNodeGrouping>>& InAvailableGroupings, TArray<TSharedPtr<FTreeNodeGrouping>>& InOutCurrentGroupings) const override
		{
			InOutCurrentGroupings.Reset();

			check(InAvailableGroupings[0]->Is<FTreeNodeGroupingFlat>());
			InOutCurrentGroupings.Add(InAvailableGroupings[0]);

			const TSharedPtr<FTreeNodeGrouping>* SizeGrouping = InAvailableGroupings.FindByPredicate(
				[](TSharedPtr<FTreeNodeGrouping>& Grouping)
				{
					return Grouping->Is<FMemAllocGroupingBySize>();
				});
			if (SizeGrouping)
			{
				InOutCurrentGroupings.Add(*SizeGrouping);
			}
		}
		virtual void GetColumnConfigSet(TArray<FTableColumnConfig>& InOutConfigSet) const override
		{
			InOutConfigSet.Add({ FTable::GetHierarchyColumnId(),          true, 200.0f });
			InOutConfigSet.Add({ FMemAllocTableColumns::AddressColumnId,  true, 120.0f });
			InOutConfigSet.Add({ FMemAllocTableColumns::CountColumnId,    true, 100.0f });
			InOutConfigSet.Add({ FMemAllocTableColumns::SizeColumnId,     true, 100.0f });
			InOutConfigSet.Add({ FMemAllocTableColumns::TagColumnId,      true, 120.0f });
			InOutConfigSet.Add({ FMemAllocTableColumns::FunctionColumnId, true, 400.0f });
		}
	};
	AvailableViewPresets.Add(MakeShared<FSizeViewPreset>());

	//////////////////////////////////////////////////
	// Tag Breakdown View

	class FTagViewPreset : public ITableTreeViewPreset
	{
	public:
		virtual FText GetName() const override
		{
			return LOCTEXT("Tag_PresetName", "Tags");
		}
		virtual FText GetToolTip() const override
		{
			return LOCTEXT("Tag_PresetToolTip", "Tag Breakdown View\nConfigure the tree view to show a breakdown of allocations by their LLM tag.");
		}
		virtual FName GetSortColumn() const override
		{
			return FTable::GetHierarchyColumnId();
		}
		virtual EColumnSortMode::Type GetSortMode() const override
		{
			return EColumnSortMode::Type::Ascending;
		}
		virtual void SetCurrentGroupings(const TArray<TSharedPtr<FTreeNodeGrouping>>& InAvailableGroupings, TArray<TSharedPtr<FTreeNodeGrouping>>& InOutCurrentGroupings) const override
		{
			InOutCurrentGroupings.Reset();

			check(InAvailableGroupings[0]->Is<FTreeNodeGroupingFlat>());
			InOutCurrentGroupings.Add(InAvailableGroupings[0]);

			const TSharedPtr<FTreeNodeGrouping>* TagGrouping = InAvailableGroupings.FindByPredicate(
				[](TSharedPtr<FTreeNodeGrouping>& Grouping)
				{
					return Grouping->Is<FMemAllocGroupingByTag>();
				});
			if (TagGrouping)
			{
				InOutCurrentGroupings.Add(*TagGrouping);
			}
		}
		virtual void GetColumnConfigSet(TArray<FTableColumnConfig>& InOutConfigSet) const override
		{
			InOutConfigSet.Add({ FTable::GetHierarchyColumnId(),          true, 200.0f });
			InOutConfigSet.Add({ FMemAllocTableColumns::CountColumnId,    true, 100.0f });
			InOutConfigSet.Add({ FMemAllocTableColumns::SizeColumnId,     true, 100.0f });
			InOutConfigSet.Add({ FMemAllocTableColumns::FunctionColumnId, true, 400.0f });
		}
	};
	AvailableViewPresets.Add(MakeShared<FTagViewPreset>());

	//////////////////////////////////////////////////
	// Asset Breakdown View

	class FAssetViewPreset : public ITableTreeViewPreset
	{
	public:
		virtual FText GetName() const override
		{
			return LOCTEXT("Asset_PresetName", "Asset");
		}
		virtual FText GetToolTip() const override
		{
			return LOCTEXT("Asset_PresetToolTip", "Asset Breakdown View\nConfigure the tree view to show a breakdown of allocations by their Asset tag.");
		}
		virtual FName GetSortColumn() const override
		{
			return FTable::GetHierarchyColumnId();
		}
		virtual EColumnSortMode::Type GetSortMode() const override
		{
			return EColumnSortMode::Type::Ascending;
		}
		virtual void SetCurrentGroupings(const TArray<TSharedPtr<FTreeNodeGrouping>>& InAvailableGroupings, TArray<TSharedPtr<FTreeNodeGrouping>>& InOutCurrentGroupings) const override
		{
			InOutCurrentGroupings.Reset();

			check(InAvailableGroupings[0]->Is<FTreeNodeGroupingFlat>());
			InOutCurrentGroupings.Add(InAvailableGroupings[0]);

			const TSharedPtr<FTreeNodeGrouping>* AssetGrouping = InAvailableGroupings.FindByPredicate(
				[](TSharedPtr<FTreeNodeGrouping>& Grouping)
				{
					return Grouping->Is<FTreeNodeGroupingByPathBreakdown>() &&
						   Grouping->As<FTreeNodeGroupingByPathBreakdown>().GetColumnId() == FMemAllocTableColumns::AssetColumnId;
				});
			if (AssetGrouping)
			{
				InOutCurrentGroupings.Add(*AssetGrouping);
			}
		}
		virtual void GetColumnConfigSet(TArray<FTableColumnConfig>& InOutConfigSet) const override
		{
			InOutConfigSet.Add({ FTable::GetHierarchyColumnId(),          true, 200.0f });
			InOutConfigSet.Add({ FMemAllocTableColumns::CountColumnId,    true, 100.0f });
			InOutConfigSet.Add({ FMemAllocTableColumns::SizeColumnId,     true, 100.0f });
			InOutConfigSet.Add({ FMemAllocTableColumns::TagColumnId,      true, 120.0f });
			InOutConfigSet.Add({ FMemAllocTableColumns::FunctionColumnId, true, 400.0f });
		}
	};
	AvailableViewPresets.Add(MakeShared<FAssetViewPreset>());

	//////////////////////////////////////////////////
	// Class Name Breakdown View

	class FClassNameViewPreset : public ITableTreeViewPreset
	{
	public:
		virtual FText GetName() const override
		{
			return LOCTEXT("ClassName_PresetName", "Class Name");
		}
		virtual FText GetToolTip() const override
		{
			return LOCTEXT("ClassName_PresetToolTip", "Class Breakdown View\nConfigure the tree view to show a breakdown of allocations by their Class name.");
		}
		virtual FName GetSortColumn() const override
		{
			return FTable::GetHierarchyColumnId();
		}
		virtual EColumnSortMode::Type GetSortMode() const override
		{
			return EColumnSortMode::Type::Ascending;
		}
		virtual void SetCurrentGroupings(const TArray<TSharedPtr<FTreeNodeGrouping>>& InAvailableGroupings, TArray<TSharedPtr<FTreeNodeGrouping>>& InOutCurrentGroupings) const override
		{
			InOutCurrentGroupings.Reset();

			check(InAvailableGroupings[0]->Is<FTreeNodeGroupingFlat>());
			InOutCurrentGroupings.Add(InAvailableGroupings[0]);

			const TSharedPtr<FTreeNodeGrouping>* ClassNameGrouping = InAvailableGroupings.FindByPredicate(
				[](TSharedPtr<FTreeNodeGrouping>& Grouping)
				{
					return Grouping->Is<FTreeNodeGroupingByUniqueValueCString>() &&
						   Grouping->As<FTreeNodeGroupingByUniqueValueCString>().GetColumnId() == FMemAllocTableColumns::ClassNameColumnId;
				});
			if (ClassNameGrouping)
			{
				InOutCurrentGroupings.Add(*ClassNameGrouping);
			}

			const TSharedPtr<FTreeNodeGrouping>* AssetGrouping = InAvailableGroupings.FindByPredicate(
				[](TSharedPtr<FTreeNodeGrouping>& Grouping)
				{
					return Grouping->Is<FTreeNodeGroupingByUniqueValueCString>() &&
						   Grouping->As<FTreeNodeGroupingByUniqueValueCString>().GetColumnId() == FMemAllocTableColumns::AssetColumnId;
				});
			if (AssetGrouping)
			{
				InOutCurrentGroupings.Add(*AssetGrouping);
			}
		}
		virtual void GetColumnConfigSet(TArray<FTableColumnConfig>& InOutConfigSet) const override
		{
			InOutConfigSet.Add({ FTable::GetHierarchyColumnId(),          true, 200.0f });
			InOutConfigSet.Add({ FMemAllocTableColumns::CountColumnId,    true, 100.0f });
			InOutConfigSet.Add({ FMemAllocTableColumns::SizeColumnId,     true, 100.0f });
			InOutConfigSet.Add({ FMemAllocTableColumns::TagColumnId,      true, 120.0f });
			InOutConfigSet.Add({ FMemAllocTableColumns::FunctionColumnId, true, 400.0f });
		}
	};
	AvailableViewPresets.Add(MakeShared<FClassNameViewPreset>());

	//////////////////////////////////////////////////
	// (Inverted) Callstack Breakdown View

	class FCallstackViewPreset : public ITableTreeViewPreset
	{
	public:
		FCallstackViewPreset(bool bIsInverted)
			: bIsInvertedCallstack(bIsInverted)
		{
		}

		virtual FText GetName() const override
		{
			return bIsInvertedCallstack ?
				LOCTEXT("InvertedCallstack_PresetName", "Inverted Callstack") :
				LOCTEXT("Callstack_PresetName", "Callstack");
		}
		virtual FText GetToolTip() const override
		{
			return bIsInvertedCallstack ?
				LOCTEXT("InvertedCallstack_PresetToolTip", "Inverted Callstack Breakdown View\nConfigure the tree view to show a breakdown of allocations by inverted callstack.") :
				LOCTEXT("Callstack_PresetToolTip", "Callstack Breakdown View\nConfigure the tree view to show a breakdown of allocations by callstack.");
		}
		virtual FName GetSortColumn() const override
		{
			return FMemAllocTableColumns::SizeColumnId;
		}
		virtual EColumnSortMode::Type GetSortMode() const override
		{
			return EColumnSortMode::Type::Descending;
		}
		virtual void SetCurrentGroupings(const TArray<TSharedPtr<FTreeNodeGrouping>>& InAvailableGroupings, TArray<TSharedPtr<FTreeNodeGrouping>>& InOutCurrentGroupings) const override
		{
			InOutCurrentGroupings.Reset();

			check(InAvailableGroupings[0]->Is<FTreeNodeGroupingFlat>());
			InOutCurrentGroupings.Add(InAvailableGroupings[0]);

			const bool bIsInverted = bIsInvertedCallstack;
			const TSharedPtr<FTreeNodeGrouping>* CallstackGrouping = InAvailableGroupings.FindByPredicate(
				[bIsInverted](TSharedPtr<FTreeNodeGrouping>& Grouping)
				{
					return Grouping->Is<FMemAllocGroupingByCallstack>() &&
						   Grouping->As<FMemAllocGroupingByCallstack>().IsInverted() == bIsInverted;
				});
			if (CallstackGrouping)
			{
				InOutCurrentGroupings.Add(*CallstackGrouping);
			}
		}
		virtual void GetColumnConfigSet(TArray<FTableColumnConfig>& InOutConfigSet) const override
		{
			InOutConfigSet.Add({ FTable::GetHierarchyColumnId(),          true, 400.0f });
			InOutConfigSet.Add({ FMemAllocTableColumns::CountColumnId,    true, 100.0f });
			InOutConfigSet.Add({ FMemAllocTableColumns::SizeColumnId,     true, 100.0f });
			InOutConfigSet.Add({ FMemAllocTableColumns::TagColumnId,      true, 200.0f });
			InOutConfigSet.Add({ FMemAllocTableColumns::FunctionColumnId, true, 200.0f });
		}

	private:
		bool bIsInvertedCallstack;
	};
	AvailableViewPresets.Add(MakeShared<FCallstackViewPreset>(false));
	AvailableViewPresets.Add(MakeShared<FCallstackViewPreset>(true));

	//////////////////////////////////////////////////
	// Address (4K Page) Breakdown View

	class FPageViewPreset : public ITableTreeViewPreset
	{
	public:
		virtual FText GetName() const override
		{
			return LOCTEXT("Page_PresetName", "Address (4K Page)");
		}
		virtual FText GetToolTip() const override
		{
			return LOCTEXT("Page_PresetToolTip", "4K Page Breakdown View\nConfigure the tree view to show a breakdown of allocations by their address.\nIt groups allocs into 4K aligned memory pages.");
		}
		virtual FName GetSortColumn() const override
		{
			return FTable::GetHierarchyColumnId();
		}
		virtual EColumnSortMode::Type GetSortMode() const override
		{
			return EColumnSortMode::Type::Ascending;
		}
		virtual void SetCurrentGroupings(const TArray<TSharedPtr<FTreeNodeGrouping>>& InAvailableGroupings, TArray<TSharedPtr<FTreeNodeGrouping>>& InOutCurrentGroupings) const override
		{
			InOutCurrentGroupings.Reset();

			check(InAvailableGroupings[0]->Is<FTreeNodeGroupingFlat>());
			InOutCurrentGroupings.Add(InAvailableGroupings[0]);

			const TSharedPtr<FTreeNodeGrouping>* MemoryPageGrouping = InAvailableGroupings.FindByPredicate(
				[](TSharedPtr<FTreeNodeGrouping>& Grouping)
				{
					return Grouping->Is<FTreeNodeGroupingByUniqueValueInt64>() &&
						   Grouping->As<FTreeNodeGroupingByUniqueValueInt64>().GetColumnId() == FMemAllocTableColumns::MemoryPageColumnId;
				});
			if (MemoryPageGrouping)
			{
				InOutCurrentGroupings.Add(*MemoryPageGrouping);
			}
		}
		virtual void GetColumnConfigSet(TArray<FTableColumnConfig>& InOutConfigSet) const override
		{
			InOutConfigSet.Add({ FTable::GetHierarchyColumnId(),          true, 200.0f });
			InOutConfigSet.Add({ FMemAllocTableColumns::AddressColumnId,  true, 120.0f });
			InOutConfigSet.Add({ FMemAllocTableColumns::CountColumnId,    true, 100.0f });
			InOutConfigSet.Add({ FMemAllocTableColumns::SizeColumnId,     true, 100.0f });
			InOutConfigSet.Add({ FMemAllocTableColumns::TagColumnId,      true, 120.0f });
			InOutConfigSet.Add({ FMemAllocTableColumns::FunctionColumnId, true, 400.0f });
		}
	};
	AvailableViewPresets.Add(MakeShared<FPageViewPreset>());

	//////////////////////////////////////////////////

	SelectedViewPreset = AvailableViewPresets[0];
}

////////////////////////////////////////////////////////////////////////////////////////////////////

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
TSharedPtr<SWidget> SMemAllocTableTreeView::ConstructFooter()
{
	return
		SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.HAlign(HAlign_Left)
		.Padding(2.0f)
		[
			SNew(STextBlock)
			.Text(this, &SMemAllocTableTreeView::GetQueryInfo)
			.ToolTipText(this, &SMemAllocTableTreeView::GetQueryInfoTooltip)
		]

		+ SHorizontalBox::Slot()
		.HAlign(HAlign_Right)
		.Padding(2.0f)
		[
			SNew(STextBlock)
			.Text(this, &SMemAllocTableTreeView::GetSymbolResolutionStatus)
			.ToolTipText(this, &SMemAllocTableTreeView::GetSymbolResolutionTooltip)
		];
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

////////////////////////////////////////////////////////////////////////////////////////////////////

FText SMemAllocTableTreeView::GetSymbolResolutionStatus() const
{
	if (Session.IsValid())
	{
		const TraceServices::IModuleProvider* ModuleProvider = ReadModuleProvider(*Session.Get());
		if (ModuleProvider)
		{
			TraceServices::IModuleProvider::FStats Stats;
			ModuleProvider->GetStats(&Stats);
			//check(Stats.SymbolsDiscovered >= Stats.SymbolsResolved + Stats.SymbolsFailed);
			const int32 SymbolsPending = Stats.SymbolsDiscovered - Stats.SymbolsResolved - Stats.SymbolsFailed;
			if (SymbolsPending > 0)
			{
				return FText::Format(LOCTEXT("SymbolsResolved1", "Resolving {0} / {1} symbols ({2} resolved, {3} failed)"), SymbolsPending, Stats.SymbolsDiscovered, Stats.SymbolsResolved, Stats.SymbolsFailed);
			}
			else
			{
				return FText::Format(LOCTEXT("SymbolsResolved2", "{0} symbols ({1} resolved, {2} failed)"), Stats.SymbolsDiscovered, Stats.SymbolsResolved, Stats.SymbolsFailed);
			}
		}
	}

	return LOCTEXT("SymbolsResolutionNotPossible", "Symbol resolution was not possible.");
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FText SMemAllocTableTreeView::GetSymbolResolutionTooltip() const
{
	if (Session.IsValid())
	{
		const TraceServices::IModuleProvider* ModuleProvider = ReadModuleProvider(*Session.Get());
		if (ModuleProvider)
		{
			return FSymbolSearchPathsHelper::GetLocalizedSymbolSearchPathsText(ModuleProvider);
		}
	}
	return FText();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FText SMemAllocTableTreeView::GetQueryInfo() const
{
	return QueryInfo;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FText SMemAllocTableTreeView::GetQueryInfoTooltip() const
{
	return QueryInfoTooltip;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemAllocTableTreeView::InternalCreateGroupings()
{
	Insights::STableTreeView::InternalCreateGroupings();

	AvailableGroupings.RemoveAll(
		[](TSharedPtr<FTreeNodeGrouping>& Grouping)
		{
			if (Grouping->Is<FTreeNodeGroupingByUniqueValue>())
			{
				const FName ColumnId = Grouping->As<FTreeNodeGroupingByUniqueValue>().GetColumnId();
				if (ColumnId == FMemAllocTableColumns::StartEventIndexColumnId ||
					ColumnId == FMemAllocTableColumns::EndEventIndexColumnId ||
					ColumnId == FMemAllocTableColumns::CountColumnId)
				{
					return true;
				}
			}
			else if (Grouping->Is<FTreeNodeGroupingByPathBreakdown>())
			{
				const FName ColumnId = Grouping->As<FTreeNodeGroupingByPathBreakdown>().GetColumnId();
				if (ColumnId == FMemAllocTableColumns::FunctionColumnId ||
					ColumnId == FMemAllocTableColumns::ClassNameColumnId)
				{
					return true;
				}
			}
			return false;
		});

	int32 Index = 1; // after the Flat ("All") grouping

	AvailableGroupings.Insert(MakeShared<FMemAllocGroupingBySize>(), Index++);

	const TraceServices::IAllocationsProvider* AllocationsProvider = Session.IsValid() ? TraceServices::ReadAllocationsProvider(*Session.Get()) : nullptr;

	if (AllocationsProvider)
	{
		AvailableGroupings.Insert(MakeShared<FMemAllocGroupingByTag>(*AllocationsProvider), Index++);
	}

	AvailableGroupings.Insert(MakeShared<FMemAllocGroupingByCallstack>(false, bIsCallstackGroupingByFunction), Index++);
	AvailableGroupings.Insert(MakeShared<FMemAllocGroupingByCallstack>(true, bIsCallstackGroupingByFunction), Index++);

	if (AllocationsProvider)
	{
		AvailableGroupings.Insert(MakeShared<FMemAllocGroupingByHeap>(*AllocationsProvider), Index++);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemAllocTableTreeView::UpdateQueryInfo()
{
	if (Rule.IsValid())
	{
		FText TimeMarkersText;

		const int NumMarkers = Rule->GetNumTimeMarkers();
		switch (NumMarkers)
		{
		case 1:
			TimeMarkersText = FText::Format(LOCTEXT("OneTimeMarkersFmt", "A={0}"), FText::AsNumber(TimeMarkers[0]));
			break;
		case 2:
			TimeMarkersText = FText::Format(LOCTEXT("TwoTimeMarkersFmt", "A={0}  B={1}"), FText::AsNumber(TimeMarkers[0]), FText::AsNumber(TimeMarkers[1]));
			break;
		case 3:
			TimeMarkersText = FText::Format(LOCTEXT("ThreeTimeMarkersFmt", "A={0}  B={1}  C={2}"), FText::AsNumber(TimeMarkers[0]), FText::AsNumber(TimeMarkers[1]), FText::AsNumber(TimeMarkers[2]));
			break;
		case 4:
			TimeMarkersText = FText::Format(LOCTEXT("FourTimeMarkersFmt", "A={0}  B={1}  C={2}  D={3}"), FText::AsNumber(TimeMarkers[0]), FText::AsNumber(TimeMarkers[1]), FText::AsNumber(TimeMarkers[2]), FText::AsNumber(TimeMarkers[3]));
			break;
		default:
			// Unhandled value
			check(false);
		}

		QueryInfo = FText::Format(LOCTEXT("QueryInfoFmt", "{0} ({1}) : {2} allocs"), Rule->GetVerboseName(), TimeMarkersText, FText::AsNumber(TableTreeNodes.Num()));
		QueryInfoTooltip = Rule->GetDescription();
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool SMemAllocTableTreeView::ApplyCustomAdvancedFilters(const FTableTreeNodePtr& NodePtr)
{
	// Super heavy to compute, validate that the filter has a use for this key before computing it
	if (FilterConfigurator && FilterConfigurator->IsKeyUsed(FullCallStackIndex))
	{
		FMemAllocNodePtr MemNodePtr = StaticCastSharedPtr<FMemAllocNode>(NodePtr);
		Context.SetFilterData<FString>(FullCallStackIndex, MemNodePtr->GetFullCallstack().ToString());
	}

	if (FilterConfigurator && FilterConfigurator->IsKeyUsed(LLMFilterIndex))
	{
		FMemAllocNodePtr MemNodePtr = StaticCastSharedPtr<FMemAllocNode>(NodePtr);
		Context.SetFilterData<FString>(LLMFilterIndex, MemNodePtr->GetMemAlloc()->GetTag());
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemAllocTableTreeView::AddCustomAdvancedFilters()
{
	TSharedPtr<TArray<TSharedPtr<struct FFilter>>>& AvailableFilters = FilterConfigurator->GetAvailableFilters();

	AvailableFilters->Add(MakeShared<FFilter>(FullCallStackIndex, LOCTEXT("FullCallstack", "Full Callstack"), LOCTEXT("SearchFullCallstack", "Search in all the callstack frames"), EFilterDataType::String, FFilterService::Get()->GetStringOperators()));
	Context.AddFilterData<FString>(FullCallStackIndex, FString());

	TSharedPtr<FFilterWithSuggestions> LLMCategoryFilter = MakeShared<FFilterWithSuggestions>(static_cast<int32>(LLMFilterIndex), LOCTEXT("LLMTag", "LLM Tag"), LOCTEXT("LLMTag", "LLM Tag"), EFilterDataType::String, FFilterService::Get()->GetStringOperators());
	Context.AddFilterData<FString>(LLMFilterIndex, FString());
	LLMCategoryFilter->Callback = [this](const FString& Text, TArray<FString>& OutSuggestions)
	{
		this->PopulateLLMTagSuggestionList(Text, OutSuggestions);
	};

	AvailableFilters->Add(LLMCategoryFilter);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemAllocTableTreeView::PopulateLLMTagSuggestionList(const FString& Text, TArray<FString>& OutSuggestions)
{
	const TraceServices::IAllocationsProvider* AllocationsProvider = TraceServices::ReadAllocationsProvider(*Session.Get());
	if (!AllocationsProvider)
	{
		return;
	}

	TraceServices::FProviderReadScopeLock _(*AllocationsProvider);

	// Use a Set to avoid duplicate tag names.
	TSet<FString> Suggestions;
	AllocationsProvider->EnumerateTags([&Suggestions, &Text](const TCHAR* Display, const TCHAR* FullPath, TraceServices::TagIdType CurrentTag, TraceServices::TagIdType ParentTag)
	{
		if (Text.IsEmpty() || FCString::Stristr(FullPath, *Text))
		{
			Suggestions.Add(FullPath);
		}

		return true;
	});

	OutSuggestions = Suggestions.Array();
	OutSuggestions.Sort();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
TSharedRef<SWidget> SMemAllocTableTreeView::ConstructFunctionToggleButton()
{
	TSharedRef<SWidget> Widget = SNew(SCheckBox)
		.Style(FAppStyle::Get(), "ToggleButtonCheckbox")
		.HAlign(HAlign_Center)
		.Padding(FMargin(4.0f, 2.0f, 4.0f, 2.0f))
		.OnCheckStateChanged(this, &SMemAllocTableTreeView::CallstackGroupingByFunction_OnCheckStateChanged)
		.IsChecked(this, &SMemAllocTableTreeView::CallstackGroupingByFunction_IsChecked)
		.ToolTip(
			SNew(SToolTip)
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(2.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("CallstackGroupingByFunction_Tooltip_Title", "Callstack Grouping by Function Name"))
					.TextStyle(FInsightsStyle::Get(), TEXT("TreeTable.TooltipBold"))
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(2.0f, 8.0f, 2.0f, 2.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("CallstackGroupingByFunction_Tooltip_Content", "If enabled, the callstack grouping will create a single group node per function name.\nExample 1: When two callstack frames are located in same function, but at different line numbers; \nExample 2: When a function is called recursively.\nOtherwise it will create separate group nodes for each unique callstack frame."))
					.TextStyle(FInsightsStyle::Get(), TEXT("TreeTable.Tooltip"))
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(2.0f, 8.0f, 2.0f, 2.0f)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Top)
					.Padding(0.0f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("CallstackGroupingByFunction_Warning", "Warning:"))
						.TextStyle(FInsightsStyle::Get(), TEXT("TreeTable.Tooltip"))
						.ColorAndOpacity(FLinearColor(1.0f, 0.6f, 0.3f, 1.0f))
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(4.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("CallstackGroupingByFunction_Warning_Content", "When this option is enabled, the tree nodes that have merged multiple callstack frames\nwill show in their tooltips the source file name and the line number of an arbitrary\ncallstack frame from ones merged by respective tree node."))
						.TextStyle(FInsightsStyle::Get(), TEXT("TreeTable.Tooltip"))
					]
				]
			])
		[
			SNew(SImage)
			.Image(FInsightsStyle::GetBrush("Icons.Function"))
		];

	return Widget;
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemAllocTableTreeView::CallstackGroupingByFunction_OnCheckStateChanged(ECheckBoxState NewRadioState)
{
	PreChangeGroupings();
	bIsCallstackGroupingByFunction = (NewRadioState == ECheckBoxState::Checked);
	for (TSharedPtr<FTreeNodeGrouping>& Grouping : AvailableGroupings)
	{
		if (Grouping->Is<FMemAllocGroupingByCallstack>())
		{
			Grouping->As<FMemAllocGroupingByCallstack>().SetGroupingByFunction(bIsCallstackGroupingByFunction);
		}
	}
	PostChangeGroupings();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

ECheckBoxState SMemAllocTableTreeView::CallstackGroupingByFunction_IsChecked() const
{
	return bIsCallstackGroupingByFunction ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

TSharedPtr<FMemAllocNode> SMemAllocTableTreeView::GetSingleSelectedMemAllocNode() const
{
	if (TreeView->GetNumItemsSelected() == 1)
	{
		FMemAllocNodePtr SelectedTreeNode = StaticCastSharedPtr<FMemAllocNode>(TreeView->GetSelectedItems()[0]);
		if (SelectedTreeNode.IsValid() && !SelectedTreeNode->IsGroup())
		{
			return SelectedTreeNode;
		}
	}
	return nullptr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemAllocTableTreeView::ExtendMenu(FMenuBuilder& MenuBuilder)
{
	ISourceCodeAccessModule& SourceCodeAccessModule = FModuleManager::LoadModuleChecked<ISourceCodeAccessModule>("SourceCodeAccess");
	ISourceCodeAccessor& SourceCodeAccessor = SourceCodeAccessModule.GetAccessor();

	FMemAllocNodePtr SingleSelectedMemAllocNode = GetSingleSelectedMemAllocNode();
	if (SingleSelectedMemAllocNode.IsValid() && CountSourceFiles(*SingleSelectedMemAllocNode) > 0)
	{
		MenuBuilder.BeginSection("Allocation", LOCTEXT("ContextMenu_Section_OpenSource", "Allocation"));
		{
			FText ItemLabel = FText::Format(LOCTEXT("ContextMenu_Open_SubMenu", "Open in {0}"), SourceCodeAccessor.GetNameText());
			FText ItemToolTip = FText::Format(LOCTEXT("ContextMenu_Open_Desc_SubMenu", "Open source file of selected callstack frame in {0}."), SourceCodeAccessor.GetNameText());

			MenuBuilder.AddSubMenu
			(
				ItemLabel,
				ItemToolTip,
				FNewMenuDelegate::CreateSP(this, &SMemAllocTableTreeView::BuildOpenSourceSubMenu),
				false,
				FSlateIcon(FAppStyle::GetAppStyleSetName(), SourceCodeAccessor.GetOpenIconName())
			);
		}
		MenuBuilder.EndSection();
	}
	else
	{
		MenuBuilder.BeginSection("CallstackFrame", LOCTEXT("ContextMenu_Section_CallstackFrame", "Callstack Frame"));
		{
			FText ItemLabel = FText::Format(LOCTEXT("ContextMenu_Open", "Open in {0}"), SourceCodeAccessor.GetNameText());
			FText FileName = GetSelectedCallstackFrameFileName();
			FText ItemToolTip = FText::Format(LOCTEXT("ContextMenu_Open_Desc", "Open source file of selected callstack frame in {0}.\n{1}"), SourceCodeAccessor.GetNameText(), FileName);

			FUIAction Action_OpenIDE
			(
				FExecuteAction::CreateSP(this, &SMemAllocTableTreeView::OpenCallstackFrameSourceFileInIDE),
				FCanExecuteAction::CreateSP(this, &SMemAllocTableTreeView::CanOpenCallstackFrameSourceFileInIDE)
			);
			MenuBuilder.AddMenuEntry
			(
				ItemLabel,
				ItemToolTip,
				FSlateIcon(FAppStyle::GetAppStyleSetName(), SourceCodeAccessor.GetOpenIconName()),
				Action_OpenIDE,
				NAME_None,
				EUserInterfaceActionType::Button
			);
		}
		MenuBuilder.EndSection();
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

uint32 SMemAllocTableTreeView::CountSourceFiles(FMemAllocNode& MemAllocNode)
{
	if (MemAllocNode.IsGroup())
	{
		return 0;
	}

	const FMemoryAlloc* Alloc = MemAllocNode.GetMemAlloc();
	if (!Alloc || !Alloc->Callstack)
	{
		return 0;
	}

	uint32 NumSourceFiles = 0;
	const uint32 NumCallstackFrames = Alloc->Callstack->Num();
	check(NumCallstackFrames <= 256); // see Callstack->Frame(uint8)
	for (uint32 FrameIndex = 0; FrameIndex < NumCallstackFrames; ++FrameIndex)
	{
		const TraceServices::FStackFrame* Frame = Alloc->Callstack->Frame(static_cast<uint8>(FrameIndex));
		if (Frame && Frame->Symbol && Frame->Symbol->File)
		{
			++NumSourceFiles;
		}
	}
	return NumSourceFiles;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemAllocTableTreeView::BuildOpenSourceSubMenu(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.BeginSection("OpenSource");
	{
		uint32 NumSourceFiles = 0;

		FMemAllocNodePtr MemAllocNode = GetSingleSelectedMemAllocNode();
		if (MemAllocNode.IsValid())
		{
			const FMemoryAlloc* Alloc = MemAllocNode->GetMemAlloc();
			if (Alloc && Alloc->Callstack)
			{
				ISourceCodeAccessModule& SourceCodeAccessModule = FModuleManager::LoadModuleChecked<ISourceCodeAccessModule>("SourceCodeAccess");
				ISourceCodeAccessor& SourceCodeAccessor = SourceCodeAccessModule.GetAccessor();

				const uint32 NumCallstackFrames = Alloc->Callstack->Num();
				check(NumCallstackFrames <= 256); // see Callstack->Frame(uint8)
				for (uint32 FrameIndex = 0; FrameIndex < NumCallstackFrames; ++FrameIndex)
				{
					const TraceServices::FStackFrame* Frame = Alloc->Callstack->Frame(static_cast<uint8>(FrameIndex));
					if (Frame && Frame->Symbol && Frame->Symbol->File)
					{
						FText ItemLabel;
						FText ItemToolTip;
						if (Frame->Symbol->GetResult() == TraceServices::ESymbolQueryResult::OK)
						{
							FText FileName;
							constexpr int32 MaxFileNameLen = 120;
							const int32 FileNameLen = FCString::Strlen(Frame->Symbol->File);
							if (FileNameLen > MaxFileNameLen)
							{
								FString FileNameStr = TEXT("...") + FString(MaxFileNameLen, Frame->Symbol->File + (FileNameLen - MaxFileNameLen));
								FileName = FText::FromString(FileNameStr);
							}
							else
							{
								FileName = FText::FromString(Frame->Symbol->File);
							}

							FText SymbolName;
							constexpr int32 MaxSymbolNameLen = 100;
							const int32 SymbolNameLen = FCString::Strlen(Frame->Symbol->Name);
							if (SymbolNameLen > MaxSymbolNameLen)
							{
								FString SymbolNameStr = TEXT("...") + FString(MaxSymbolNameLen, Frame->Symbol->Name + (SymbolNameLen - MaxSymbolNameLen));
								SymbolName = FText::FromString(SymbolNameStr);
							}
							else
							{
								SymbolName = FText::FromString(Frame->Symbol->Name);
							}

							ItemLabel = FText::Format(LOCTEXT("ContextMenu_OpenSource_Fmt1", "{0} ({1}) \u2192 {2}"),
								FileName,
								FText::AsNumber(Frame->Symbol->Line),
								SymbolName);

							ItemToolTip = FText::Format(LOCTEXT("ContextMenu_OpenSource_Desc_Fmt1", "Open source file of selected callstack frame in {0}.\n{1} (line {2})\n\u2192 {3}"),
								SourceCodeAccessor.GetNameText(),
								FText::FromString(Frame->Symbol->File),
								FText::AsNumber(Frame->Symbol->Line),
								FText::FromString(Frame->Symbol->Name));
						}
						else
						{
							ItemLabel = FText::Format(LOCTEXT("ContextMenu_OpenSource_Fmt2", "{0} ({1}) \u2192 {2}"),
								FText::FromString(Frame->Symbol->Module),
								FText::FromString(FString::Printf(TEXT("0x%X"), Frame->Addr)),
								FText::FromString(TraceServices::QueryResultToString(Frame->Symbol->GetResult())));

							ItemToolTip = FText::Format(LOCTEXT("ContextMenu_OpenSource_Desc_Fmt2", "Open source file of selected callstack frame in {0}."),
								SourceCodeAccessor.GetNameText());
						}

						const bool bFileExists = FPaths::FileExists(Frame->Symbol->File);

						FUIAction Action_OpenIDE
						(
							FExecuteAction::CreateSP(this, &SMemAllocTableTreeView::OpenSourceFileInIDE, Frame->Symbol->File, uint32(Frame->Symbol->Line)),
							FCanExecuteAction::CreateLambda([bFileExists]() { return bFileExists; })
						);
						MenuBuilder.AddMenuEntry
						(
							ItemLabel,
							ItemToolTip,
							FSlateIcon(),
							Action_OpenIDE,
							NAME_None,
							EUserInterfaceActionType::Button
						);

						++NumSourceFiles;
					}
				}
			}
		}

		if (NumSourceFiles == 0)
		{
			struct FLocal
			{
				static bool ReturnFalse()
				{
					return false;
				}
			};

			FUIAction DummyUIAction;
			DummyUIAction.CanExecuteAction = FCanExecuteAction::CreateStatic(&FLocal::ReturnFalse);

			MenuBuilder.AddMenuEntry
			(
				LOCTEXT("ContextMenu_OpenSourceNA", "No Source File Available"),
				TAttribute<FText>(),
				FSlateIcon(),
				DummyUIAction,
				NAME_None,
				EUserInterfaceActionType::None
			);
		}
	}
	MenuBuilder.EndSection();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemAllocTableTreeView::OpenSourceFileInIDE(const TCHAR* InFile, uint32 Line) const
{
	ISourceCodeAccessModule& SourceCodeAccessModule = FModuleManager::LoadModuleChecked<ISourceCodeAccessModule>("SourceCodeAccess");

	const FString File = InFile;

	if (FPaths::FileExists(File))
	{
		ISourceCodeAccessor& SourceCodeAccessor = SourceCodeAccessModule.GetAccessor();
		SourceCodeAccessor.OpenFileAtLine(File, Line);
	}
	else
	{
		SourceCodeAccessModule.OnOpenFileFailed().Broadcast(File);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool SMemAllocTableTreeView::CanOpenCallstackFrameSourceFileInIDE() const
{
	if (TreeView->GetNumItemsSelected() != 1)
	{
		return false;
	}

	FTableTreeNodePtr TreeNode = TreeView->GetSelectedItems()[0];
	return TreeNode.IsValid() && TreeNode->IsGroup() && TreeNode->GetContext();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemAllocTableTreeView::OpenCallstackFrameSourceFileInIDE()
{
	if (TreeView->GetNumItemsSelected() > 0)
	{
		FTableTreeNodePtr TreeNode = TreeView->GetSelectedItems()[0];
		if (TreeNode.IsValid() && TreeNode->IsGroup() && TreeNode->GetContext())
		{
			const TraceServices::FStackFrame* Frame = (const TraceServices::FStackFrame*)TreeNode->GetContext();

			if (Frame->Symbol && Frame->Symbol->File)
			{
				OpenSourceFileInIDE(Frame->Symbol->File, Frame->Symbol->Line);
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FText SMemAllocTableTreeView::GetSelectedCallstackFrameFileName() const
{
	if (TreeView->GetNumItemsSelected() > 0)
	{
		FTableTreeNodePtr TreeNode = TreeView->GetSelectedItems()[0];
		if (TreeNode.IsValid() && TreeNode->IsGroup() && TreeNode->GetContext())
		{
			const TraceServices::FStackFrame* Frame = (const TraceServices::FStackFrame*)TreeNode->GetContext();
			if (Frame->Symbol && Frame->Symbol->File)
			{
				FString SourceFileAndLine = FString::Printf(TEXT("%s(%d)"), Frame->Symbol->File, Frame->Symbol->Line);
				return FText::FromString(SourceFileAndLine);
			}
			else
			{
				return LOCTEXT("NoSourceFile", "(source file not available)");
			}
		}
	}
	return LOCTEXT("NoCallstackFrame", "(only for resolved callstack frames)");
}

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace Insights

#undef LOCTEXT_NAMESPACE
