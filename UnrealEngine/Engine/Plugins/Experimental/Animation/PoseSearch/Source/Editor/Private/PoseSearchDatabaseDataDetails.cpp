// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearchDatabaseDataDetails.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "PoseSearch/PoseSearchDefines.h"
#include "PoseSearch/PoseSearchDerivedData.h"
#include "PoseSearch/PoseSearchSchema.h"
#include "PoseSearchDatabaseViewModel.h"
#include "Widgets/Layout/SScrollBox.h"

#define LOCTEXT_NAMESPACE "PoseSearchDatabaseDataDetails"

namespace UE::PoseSearch
{

class FChannelItem
{
public:
	FChannelItem(const UPoseSearchFeatureChannel* Channel, int32 ChannelComponentIdx = -1)
	: Label(ComputeLabel(Channel, ChannelComponentIdx))
	, DataOffset(ComputeDataOffset(Channel, ChannelComponentIdx))
	, Cardinality(ComputeCardinality(Channel, ChannelComponentIdx))
	{
	}

	TArray<FChannelItemPtr>& GetChannelItems()
	{
		return ChannelItems;
	}

	FString GetLabel() const
	{
		return Label;
	}

	int32 GetDataOffset() const
	{
		return DataOffset;
	}

	int32 GetCardinality() const
	{
		return Cardinality;
	}

	bool IsExpanded() const
	{
		return bExpanded;
	}

	void SetExpanded(bool bValue)
	{
		bExpanded = bValue;
	}

private:
	static FString ComputeLabel(const UPoseSearchFeatureChannel* Channel, int32 ChannelComponentIdx)
	{
		if (ChannelComponentIdx >= 0)
		{
			switch (ChannelComponentIdx)
			{
			case 0: return FString("x");
			case 1: return FString("y");
			case 2: return FString("z");
			case 3: return FString("w");
			default: return FString::Printf(TEXT("%d"), ChannelComponentIdx);
			}
		}

		if (Channel != nullptr)
		{
			return Channel->GetLabel();
		}

		return FString();
	}

	static int32 ComputeDataOffset(const UPoseSearchFeatureChannel* Channel, int32 ChannelComponentIdx)
	{
		int32 DataOffset = 0;

		if (Channel != nullptr)
		{
			DataOffset = Channel->GetChannelDataOffset();

			if (ChannelComponentIdx > 0)
			{
				DataOffset += ChannelComponentIdx;
			}
		}

		return DataOffset;
	}

	static int32 ComputeCardinality(const UPoseSearchFeatureChannel* Channel, int32 ChannelComponentIdx)
	{
		int32 Cardinality = 0;

		if (ChannelComponentIdx >= 0)
		{
			Cardinality = 1;
		}
		else if (Channel != nullptr)
		{
			Cardinality = Channel->GetChannelCardinality();
		}

		return Cardinality;
	}

	const FString Label;
	const int32 DataOffset = 0;
	const int32 Cardinality = 0;
	bool bExpanded = false;
	TArray<FChannelItemPtr> ChannelItems;
};

class SDatabaseDataDetailsTableRow : public SMultiColumnTableRow<FChannelItemPtr>
{
	FChannelItemPtr ChannelItem;
	TWeakPtr<FDatabaseViewModel> EditorViewModel;

public:
	void Construct(const FTableRowArgs& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView, FChannelItemPtr InChannelItem, TSharedRef<FDatabaseViewModel> InEditorViewModel)
	{
		EditorViewModel = InEditorViewModel;
		ChannelItem = InChannelItem;
		SMultiColumnTableRow<FChannelItemPtr>::Construct(InArgs, InOwnerTableView);
	}

	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override
	{
		if (ColumnName == FName("ChannelName"))
		{
			// Rows in a TreeView need an expander button and some indentation
			return SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Fill)
				[
					SNew(SExpanderArrow, SharedThis(this))
					.StyleSet(ExpanderStyleSet)
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1)
				.Padding(FMargin(4.0f, 0.0f))
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(ChannelItem->GetLabel()))
				];
		}
		
		if (ColumnName == FName("DataOffset"))
		{
			return SNew(STextBlock)
			.Text(FText::FromString(FString::Printf(TEXT("%d"), ChannelItem->GetDataOffset())));
		}

		if (ColumnName == FName("Query"))
		{
			return SNew(STextBlock)
			.Text_Lambda([this, ColumnName]() -> FText
			{
				TStringBuilder<256> StringBuilder;
				if (!ChannelItem->IsExpanded())
				{
					if (const TSharedPtr<FDatabaseViewModel> ViewModel = EditorViewModel.Pin())
					{
						const int32 DataOffset = ChannelItem->GetDataOffset();
						const int32 Cardinality = ChannelItem->GetCardinality();

						const TConstArrayView<float> QueryValues = ViewModel->GetQueryVector();
						if (DataOffset + Cardinality <= QueryValues.Num())
						{
							if (Cardinality > 1)
							{
								// using only one decimal to keep the string compact
								for (int32 i = 0; i < Cardinality; ++i)
								{
									if (i != 0)
									{
										StringBuilder.Append(TEXT(", "));
									}
									const float Value = QueryValues[i + DataOffset];
									StringBuilder.Appendf(TEXT("%.1f"), Value);
								}
							}
							else
							{
								// using all the float digits 
								const float Value = QueryValues[DataOffset];
								StringBuilder.Appendf(TEXT("%f"), Value);
							}
						}
					}
				}
				return FText::FromString(StringBuilder.ToString());
			});
		}
		
		return SNew(STextBlock)
		.Margin(FMargin(1.f, 1.f, 1.f, 1.f))
		.Text_Lambda([this, ColumnName]() -> FText
		{
			TStringBuilder<256> StringBuilder;
			
			if (!ChannelItem->IsExpanded())
			{
				if (const TSharedPtr<FDatabaseViewModel> ViewModel = EditorViewModel.Pin())
				{
					const UPoseSearchDatabase* PoseSearchDatabase = ViewModel->GetPoseSearchDatabase();
					if (FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(PoseSearchDatabase, ERequestAsyncBuildFlag::ContinueRequest))
					{
						if (const FDatabasePreviewActor* FoundPreviewActor = ViewModel->GetPreviewActors().FindByPredicate(
							[ColumnName](const FDatabasePreviewActor& PreviewActor)
							{ return *PreviewActor.Actor->GetName() == ColumnName; }))
						{
							const int32 PoseIdx = FoundPreviewActor->CurrentPoseIndex;
							const TArray<float> PoseValues = PoseSearchDatabase->GetSearchIndex().GetPoseValuesSafe(PoseIdx);

							const int32 DataOffset = ChannelItem->GetDataOffset();
							const int32 Cardinality = ChannelItem->GetCardinality();

							if (Cardinality > 1)
							{
								// using only one decimal to keep the string compact
								for (int32 i = 0; i < Cardinality; ++i)
								{
									if (i != 0)
									{
										StringBuilder.Append(TEXT(", "));
									}
									const float Value = PoseValues[i + DataOffset];
									StringBuilder.Appendf(TEXT("%.1f"), Value);
								}
							}
							else
							{
								// using all the float digits 
								const float Value = PoseValues[DataOffset];
								StringBuilder.Appendf(TEXT("%f"), Value);
							}
						}
					}
				}
			}
			return FText::FromString(StringBuilder.ToString());
		});
	}
};

void SDatabaseDataDetails::Construct(const FArguments& Args, TSharedRef<FDatabaseViewModel> InEditorViewModel)
{
	EditorViewModel = InEditorViewModel;
}

void SDatabaseDataDetails::TrackExpandedItems(const TArray<FChannelItemPtr>& ChannelItems, TMap<const FString, bool>& ExpandedItems)
{
	for (const FChannelItemPtr& ChannelItem : ChannelItems)
	{
		TrackExpandedItems(ChannelItem->GetChannelItems(), ExpandedItems);
		ExpandedItems.FindOrAdd(ChannelItem->GetLabel()) = ChannelItem->IsExpanded();
	}
}

void SDatabaseDataDetails::SetExpandedItems(TArray<FChannelItemPtr>& ChannelItems, const TMap<const FString, bool>& ExpandedItems, SChannelItemsTreeView* ChannelItemsTreeView)
{
	for (const FChannelItemPtr& ChannelItem : ChannelItems)
	{
		SetExpandedItems(ChannelItem->GetChannelItems(), ExpandedItems, ChannelItemsTreeView);
		
		bool bIsExpanded = false;
		if (const bool* IsExpandedPtr = ExpandedItems.Find(ChannelItem->GetLabel()))
		{
			bIsExpanded = *IsExpandedPtr;
		}

		ChannelItemsTreeView->SetItemExpansion(ChannelItem, bIsExpanded);
	}
};

void SDatabaseDataDetails::Reconstruct(int32 MaxPreviewActors)
{
	const TSharedPtr<FDatabaseViewModel> ViewModel = EditorViewModel.Pin();
	
	if (!ViewModel)
	{
		ChannelItems.Reset();
		return;
	}
	
	const UPoseSearchDatabase* PoseSearchDatabase = ViewModel->GetPoseSearchDatabase();
	if (!FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(PoseSearchDatabase, ERequestAsyncBuildFlag::ContinueRequest))
	{
		ChannelItems.Reset();
		return;
	}

	TMap<const FString, bool> ExpandedItems;
	TrackExpandedItems(ChannelItems, ExpandedItems);

	ChannelItems.Reset();

	RebuildChannelItemsTreeRecursively(ChannelItems, PoseSearchDatabase->Schema->GetChannels());

	// generating the header
	TSharedPtr<SHeaderRow> HeaderRow = SNew(SHeaderRow);
	HeaderRow->AddColumn(
		SHeaderRow::Column(TEXT("ChannelName"))
			.DefaultLabel(LOCTEXT("ChannelName_Header", "Channel Name"))
			.ToolTipText(LOCTEXT("ChannelName_ToolTip", "Channel Name"))
			.FillWidth(0.2f));

	HeaderRow->AddColumn(
		SHeaderRow::Column(TEXT("DataOffset"))
		.DefaultLabel(LOCTEXT("DataOffset_Header", "Data Offset"))
		.ToolTipText(LOCTEXT("DataOffset_ToolTip", "Offset from the beginning of the features data"))
		.FillWidth(0.1f));

	if (ViewModel->ShouldDrawQueryVector())
	{
		HeaderRow->AddColumn(
			SHeaderRow::Column(TEXT("Query"))
			.DefaultLabel(LOCTEXT("Query_Header", "Query"))
			.ToolTipText(LOCTEXT("Query_ToolTip", "Query Values")));
	}

	const TArray<FDatabasePreviewActor>& PreviewActors = ViewModel->GetPreviewActors();
	const int32 PreviewActorNum = FMath::Min(MaxPreviewActors, PreviewActors.Num());
	for (int32 PreviewActorIdx = 0; PreviewActorIdx < PreviewActorNum; ++PreviewActorIdx)
	{
		const FDatabasePreviewActor& PreviewActor = PreviewActors[PreviewActorIdx];
		if (PreviewActor.IsValid() && PreviewActor.Sampler.IsInitialized())
		{
			HeaderRow->AddColumn(
				SHeaderRow::Column(*PreviewActor.Actor->GetName())
				.DefaultLabel(FText::FromString(PreviewActor.Sampler.GetAsset()->GetName())));
		}
	}

	ChannelItemsTreeView = SNew(SChannelItemsTreeView)
		.TreeItemsSource(&ChannelItems)
		.HeaderRow(HeaderRow)
		.OnGenerateRow_Lambda([this](FChannelItemPtr ChannelItem, const TSharedRef<STableViewBase>& OwnerTable)
		{
			return SNew(SDatabaseDataDetailsTableRow, OwnerTable, ChannelItem, EditorViewModel.Pin().ToSharedRef());
		})
		.OnGetChildren_Lambda([](FChannelItemPtr ChannelItem, TArray<FChannelItemPtr>& OutChildren)
		{
			OutChildren.Append(ChannelItem->GetChannelItems());
		})
		.OnExpansionChanged_Lambda([](FChannelItemPtr ChannelItem, bool bExpanded)
		{
			ChannelItem->SetExpanded(bExpanded);
		});

	ChildSlot
	[
		SNew(SScrollBox)
		.Orientation(Orient_Horizontal)
		+ SScrollBox::Slot()
		.FillSize(1.f)
		[
			ChannelItemsTreeView.ToSharedRef()
		]
	];

	SetExpandedItems(ChannelItems, ExpandedItems, ChannelItemsTreeView.Get());
}

void SDatabaseDataDetails::RebuildChannelItemsTreeRecursively(TArray<FChannelItemPtr>& ChannelItems, TConstArrayView<TObjectPtr<UPoseSearchFeatureChannel>> Channels)
{
	for (const TObjectPtr<UPoseSearchFeatureChannel>& ChannelPtr : Channels)
	{
		if (ChannelPtr)
		{
			TSharedRef<FChannelItem> ChannelItem = MakeShareable(new FChannelItem(ChannelPtr));
			ChannelItems.Add(ChannelItem);

			if (ChannelPtr->GetSubChannels().IsEmpty())
			{
				for (int32 ChannelComponentIdx = 0; ChannelComponentIdx < ChannelPtr->GetChannelCardinality(); ++ChannelComponentIdx)
				{
					TSharedRef<FChannelItem> SubChannelItem = MakeShareable(new FChannelItem(ChannelPtr, ChannelComponentIdx));
					ChannelItem->GetChannelItems().Add(SubChannelItem);
				}
			}
			else
			{
				RebuildChannelItemsTreeRecursively(ChannelItem->GetChannelItems(), ChannelPtr->GetSubChannels());
			}
		}
	}
}

} // namespace UE::PoseSearch

#undef LOCTEXT_NAMESPACE
