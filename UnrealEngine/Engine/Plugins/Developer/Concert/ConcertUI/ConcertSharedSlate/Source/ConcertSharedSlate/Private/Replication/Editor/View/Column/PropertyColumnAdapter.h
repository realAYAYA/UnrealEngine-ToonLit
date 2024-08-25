// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Replication/Editor/Model/ReplicatedPropertyData.h"
#include "Replication/Editor/View/Column/IPropertyTreeColumn.h"
#include "Replication/Editor/View/Column/IReplicationTreeColumn.h"

namespace UE::ConcertSharedSlate
{
	/** Adapts an IPropertyTreeColumn to IReplicationTreeColumn<FReplicatedPropertyData>. It simply passes additional info down to IPropertyTreeColumn. */
	class FPropertyColumnAdapter : public IReplicationTreeColumn<FReplicatedPropertyData>
	{
	public:

		static TArray<TReplicationColumnEntry<FReplicatedPropertyData>> Transform(const TArray<FPropertyColumnEntry>& Entries)
		{
			TArray<TReplicationColumnEntry<FReplicatedPropertyData>> Result;
			Algo::Transform(Entries, Result, [](const FPropertyColumnEntry& Entry) -> TReplicationColumnEntry<FReplicatedPropertyData>
			{
				return {
					TReplicationColumnDelegates<FReplicatedPropertyData>::FCreateColumn::CreateLambda([CreateDelegate = Entry.CreateColumn]()
					{
						return MakeShared<FPropertyColumnAdapter>(CreateDelegate.Execute());
					}),
					Entry.ColumnId,
					Entry.ColumnInfo
				};
			});
			return Result;
		}
		
		FPropertyColumnAdapter(TSharedRef<IPropertyTreeColumn> InAdaptedColumn)
			: AdaptedColumn(MoveTemp(InAdaptedColumn))
		{}
		
		virtual SHeaderRow::FColumn::FArguments CreateHeaderRowArgs() const override { return AdaptedColumn->CreateHeaderRowArgs(); }
		virtual TSharedRef<SWidget> GenerateColumnWidget(const FBuildArgs& InArgs) override
		{
			return AdaptedColumn->GenerateColumnWidget({ InArgs.HighlightText, Transform(InArgs.RowItem) });
		}
		virtual void PopulateSearchString(const FReplicatedPropertyData& InItem, TArray<FString>& InOutSearchStrings) const override
		{
			return AdaptedColumn->PopulateSearchString(Transform(InItem), InOutSearchStrings);
		}

		virtual bool CanBeSorted() const override { return AdaptedColumn->CanBeSorted(); }
		virtual bool IsLessThan(const FReplicatedPropertyData& Left, const FReplicatedPropertyData& Right) const override { return AdaptedColumn->IsLessThan(Transform(Left), Transform(Right)); }

	private:

		TSharedRef<IPropertyTreeColumn> AdaptedColumn;

		static FPropertyTreeRowContext Transform(FReplicatedPropertyData Data)
		{
			return { MoveTemp(Data) };
		}
	};
}
