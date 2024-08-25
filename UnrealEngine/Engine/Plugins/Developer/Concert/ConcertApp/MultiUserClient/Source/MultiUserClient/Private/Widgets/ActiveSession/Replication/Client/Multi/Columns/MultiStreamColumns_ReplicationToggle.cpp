// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultiStreamColumns.h"

#include "MultiUserReplicationStyle.h"
#include "SReplicationMultiToggleCheckbox.h"
#include "Replication/Editor/View/Column/IObjectTreeColumn.h"

#include "Widgets/Layout/SBox.h"
#include "Widgets/Views/SHeaderRow.h"

#define LOCTEXT_NAMESPACE "ReplicationToggle"

namespace UE::MultiUserClient::MultiStreamColumns
{
	const FName ReplicationToggleColumnId(TEXT("ReplicationToggleColumn"));
	
	ConcertSharedSlate::FObjectColumnEntry ReplicationToggle(
		TSharedRef<IConcertClient> ConcertClient,
		TAttribute<ConcertSharedSlate::IObjectHierarchyModel*> ObjectHierarchyModelAttribute,
		FReplicationClientManager& ClientManager,
		const int32 ColumnsSortPriority
		)
	{
		class FObjectColumn_ReplicationToggle : public ConcertSharedSlate::IObjectTreeColumn
		{
		public:

			FObjectColumn_ReplicationToggle(
				TSharedRef<IConcertClient> ConcertClient,
				TAttribute<ConcertSharedSlate::IObjectHierarchyModel*> ObjectHierarchyModelAttribute,
				FReplicationClientManager& ClientManager
				)
				: ConcertClient(MoveTemp(ConcertClient))
				, ObjectHierarchyModelAttribute(MoveTemp(ObjectHierarchyModelAttribute))
				, ClientManager(ClientManager)
			{}
			
			virtual SHeaderRow::FColumn::FArguments CreateHeaderRowArgs() const override
			{
				return SHeaderRow::Column(ReplicationToggleColumnId)
					.DefaultLabel(FText::GetEmpty())
					.ToolTipText(LOCTEXT("Replicates.ToolTip", "Assign properties first.\nControls whether the object should replicate."))
					.FixedWidth(FMultiUserReplicationStyle::Get()->GetFloat(TEXT("AllClients.Object.ReplicationToggle")));
			}
			
			virtual TSharedRef<SWidget> GenerateColumnWidget(const FBuildArgs& InArgs) override
			{
				return SNew(SBox)
					.HAlign(HAlign_Left) // Warning icon is sometimes collapsed - we don't want the widget to be centered
					[
						SNew(SReplicationMultiToggleCheckbox, ClientManager, ConcertClient)
						.Object(InArgs.RowItem.RowData.GetObjectPath())
						.ObjectHierarchyModel(ObjectHierarchyModelAttribute)
					];
			}

		private:

			const TSharedRef<IConcertClient> ConcertClient;
			const TAttribute<ConcertSharedSlate::IObjectHierarchyModel*> ObjectHierarchyModelAttribute;
			FReplicationClientManager& ClientManager;
		};

		return {
			ConcertSharedSlate::TReplicationColumnDelegates<ConcertSharedSlate::FObjectTreeRowContext>::FCreateColumn::CreateLambda(
				[ConcertClient = MoveTemp(ConcertClient), ObjectHierarchyModelAttribute = MoveTemp(ObjectHierarchyModelAttribute), &ClientManager]()
				{
					return MakeShared<FObjectColumn_ReplicationToggle>(ConcertClient, ObjectHierarchyModelAttribute, ClientManager);
				}),
			ReplicationToggleColumnId,
			{ ColumnsSortPriority }
		};
	}
}

#undef LOCTEXT_NAMESPACE