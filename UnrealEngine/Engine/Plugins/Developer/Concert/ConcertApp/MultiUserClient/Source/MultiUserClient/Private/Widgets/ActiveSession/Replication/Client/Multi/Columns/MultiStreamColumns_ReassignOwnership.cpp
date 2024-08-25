// Copyright Epic Games, Inc. All Rights Reserved.

#include "IConcertClient.h"
#include "MultiStreamColumns.h"

#include "MultiUserReplicationStyle.h"
#include "SReassignObjectComboBox.h"
#include "Replication/Editor/View/IMultiReplicationStreamEditor.h"
#include "Replication/Editor/View/IReplicationStreamEditor.h"

#define LOCTEXT_NAMESPACE "ReassignOwnershipColumn"

namespace UE::MultiUserClient::MultiStreamColumns
{
	const FName ReassignOwnershipColumnId(TEXT("ReassignOwnershipColumn"));
	
	ConcertSharedSlate::FObjectColumnEntry ReassignOwnership(
		TSharedRef<IConcertClient> ConcertClient,
		TAttribute<TSharedPtr<ConcertSharedSlate::IMultiReplicationStreamEditor>> MultiStreamModelAttribute,
		TAttribute<ConcertSharedSlate::IObjectHierarchyModel*> ObjectHierarchyModelAttribute,
		FReassignObjectPropertiesLogic& ReassignmentLogic,
		const FReplicationClientManager& ClientManager,
		const int32 ColumnsSortPriority
		)
	{
		class FObjectColumn_ReassignOwnership : public ConcertSharedSlate::IObjectTreeColumn
		{
		public:

			FObjectColumn_ReassignOwnership(
				TSharedRef<IConcertClient> ConcertClient,
				TAttribute<TSharedPtr<ConcertSharedSlate::IMultiReplicationStreamEditor>> MultiStreamModelAttribute,
				TAttribute<ConcertSharedSlate::IObjectHierarchyModel*> ObjectHierarchyModelAttribute,
				FReassignObjectPropertiesLogic& ReassignmentLogic,
				const FReplicationClientManager& ClientManager
				)
				: ConcertClient(MoveTemp(ConcertClient))
				, MultiStreamModelAttribute(MoveTemp(MultiStreamModelAttribute))
				, ObjectHierarchyModelAttribute(MoveTemp(ObjectHierarchyModelAttribute))
				, ReassignmentLogic(ReassignmentLogic)
				, ClientManager(ClientManager)
			{}
			
			virtual SHeaderRow::FColumn::FArguments CreateHeaderRowArgs() const override
			{
				return SHeaderRow::Column(ReassignOwnershipColumnId)
					.DefaultLabel(LOCTEXT("Owner.Label", "Assigned Clients"))
					.ToolTipText(LOCTEXT("Owner.ToolTip", "Clients that have registered properties for an object"))
					.FillSized(FMultiUserReplicationStyle::Get()->GetFloat(TEXT("AllClients.Object.OwnerColumnWidth")));
			}
			
			virtual TSharedRef<SWidget> GenerateColumnWidget(const FBuildArgs& InArgs) override
			{
				return SNew(SReassignObjectComboBox, ConcertClient, ReassignmentLogic, ClientManager)
					.ManagedObject(InArgs.RowItem.RowData.GetObjectPath())
					.ObjectHierarchyModel(ObjectHierarchyModelAttribute)
					.HighlightText(InArgs.HighlightText)
					.OnReassignAllOptionClicked_Lambda([this](auto)
					{
						if (const TSharedPtr<ConcertSharedSlate::IMultiReplicationStreamEditor> Model = MultiStreamModelAttribute.Get())
						{
							Model->GetEditorBase().RequestObjectColumnResort(ReassignOwnershipColumnId);
						}
					});
			}
			virtual void PopulateSearchString(const ConcertSharedSlate::FObjectTreeRowContext& InItem, TArray<FString>& InOutSearchStrings) const override
			{
				SReassignObjectComboBox::PopulateSearchTerms(
					*ConcertClient->GetCurrentSession(),
					ReassignmentLogic,
					InItem.RowData.GetObjectPath(),
					InOutSearchStrings
					);
			}

			virtual bool CanBeSorted() const override { return true; }
			virtual bool IsLessThan(const ConcertSharedSlate::FObjectTreeRowContext& Left, const ConcertSharedSlate::FObjectTreeRowContext& Right) const override
			{
				const TOptional<FString> LeftClientDisplayString = SReassignObjectComboBox::GetDisplayString(ConcertClient, ReassignmentLogic, Left.RowData.GetObjectPath());
				const TOptional<FString> RightClientDisplayString = SReassignObjectComboBox::GetDisplayString(ConcertClient, ReassignmentLogic, Right.RowData.GetObjectPath());
			
				if (LeftClientDisplayString && RightClientDisplayString)
				{
					return *LeftClientDisplayString < *RightClientDisplayString;
				}
				// Our rule: set < unset. This way unassigned appears last.
				return LeftClientDisplayString.IsSet() && !RightClientDisplayString.IsSet();
			}

		private:
			
			const TSharedRef<IConcertClient> ConcertClient;
			const TAttribute<TSharedPtr<ConcertSharedSlate::IMultiReplicationStreamEditor>> MultiStreamModelAttribute;
			const TAttribute<ConcertSharedSlate::IObjectHierarchyModel*> ObjectHierarchyModelAttribute;
			FReassignObjectPropertiesLogic& ReassignmentLogic;
			const FReplicationClientManager& ClientManager;
		};

		return {
			ConcertSharedSlate::TReplicationColumnDelegates<ConcertSharedSlate::FObjectTreeRowContext>::FCreateColumn::CreateLambda(
				[ConcertClient = MoveTemp(ConcertClient), MultiStreamModelAttribute = MoveTemp(MultiStreamModelAttribute), ObjectHierarchyModelAttribute = MoveTemp(ObjectHierarchyModelAttribute), &ReassignmentLogic, &ClientManager]()
				{
					return MakeShared<FObjectColumn_ReassignOwnership>(ConcertClient, MultiStreamModelAttribute, ObjectHierarchyModelAttribute, ReassignmentLogic, ClientManager);
				}),
			ReassignOwnershipColumnId,
			{ ColumnsSortPriority }
		};
	}
}

#undef LOCTEXT_NAMESPACE