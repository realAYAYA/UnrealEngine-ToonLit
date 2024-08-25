// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultiStreamColumns.h"

#include "IConcertClient.h"
#include "MultiUserReplicationStyle.h"
#include "Replication/Client/ReplicationClient.h"
#include "Replication/Client/ReplicationClientManager.h"
#include "Replication/Editor/Model/IEditableMultiReplicationStreamModel.h"
#include "Replication/Editor/Model/IEditableReplicationStreamModel.h"
#include "Replication/Editor/View/IMultiReplicationStreamEditor.h"
#include "Replication/Editor/View/IReplicationStreamEditor.h"
#include "SAssignPropertyComboBox.h"
#include "Replication/Editor/View/Column/IPropertyTreeColumn.h"
#include "Replication/Editor/View/Column/ReplicationColumnDelegates.h"
#include "Widgets/ActiveSession/Replication/Client/ClientUtils.h"

#define LOCTEXT_NAMESPACE "AssignPropertyColumn"

namespace UE::MultiUserClient::MultiStreamColumns
{
	const FName AssignPropertyColumnId(TEXT("AssignPropertyColumn"));

	namespace AssignPropertyColumnUtils
	{
		static void ForEachStreamAssignedTo(
			const ConcertSharedSlate::IMultiReplicationStreamEditor& MultiEditor,
			const FConcertPropertyChain& Property,
			TFunctionRef<void(const TSharedRef<ConcertSharedSlate::IEditableReplicationStreamModel>& Stream)> Consume
		)
		{
			for (const TSharedRef<ConcertSharedSlate::IEditableReplicationStreamModel>& Stream : MultiEditor.GetMultiStreamModel().GetEditableStreams())
			{
				for (const FSoftObjectPath& SelectedObject : MultiEditor.GetEditorBase().GetObjectsBeingPropertyEdited())
				{
					const bool bStreamAssignedToProperty = Stream->HasProperty(SelectedObject, Property);
					if (bStreamAssignedToProperty)
					{
						Consume(Stream);
						// Each IEditableReplicationStreamModel should only be visited at most once.
						break;
					}
				}
			}
		}

		const FReplicationClient* FindClientByStream(const FReplicationClientManager& ClientManager, const ConcertSharedSlate::IReplicationStreamModel& StreamModel)
		{
			if (&ClientManager.GetLocalClient().GetClientEditModel().Get() == &StreamModel)
			{
				return &ClientManager.GetLocalClient();
			}

			for (const TNonNullPtr<const FRemoteReplicationClient> Client : ClientManager.GetRemoteClients())
			{
				if (&Client->GetClientEditModel().Get() == &StreamModel)
				{
					return Client;
				}
			}
			
			return nullptr;
		}
		
		static FString GetClientDisplayText(const IConcertClient& InConcertClient, const FReplicationClientManager& ClientManager, const ConcertSharedSlate::IReplicationStreamModel& StreamModel)
		{
			if (const FReplicationClient* Client = FindClientByStream(ClientManager, StreamModel))
			{
				return ClientUtils::GetClientDisplayName(InConcertClient, Client->GetEndpointId());
			}

			ensure(false);
			return {};
		}
	}
	
	ConcertSharedSlate::FPropertyColumnEntry AssignPropertyColumn(
		TAttribute<TSharedPtr<ConcertSharedSlate::IMultiReplicationStreamEditor>> MultiStreamEditor,
		TSharedRef<IConcertClient> ConcertClient,
		FReplicationClientManager& ClientManager,
		const int32 ColumnsSortPriority
		)
	{
		using namespace ConcertSharedSlate;
		class FPropertyColumn_AssignPropertyColumn : public IPropertyTreeColumn
		{
		public:

			FPropertyColumn_AssignPropertyColumn(
				TAttribute<TSharedPtr<IMultiReplicationStreamEditor>> MultiStreamEditor,
				TSharedRef<IConcertClient> ConcertClient,
				FReplicationClientManager& ClientManager
				)
				: MultiStreamEditor(MoveTemp(MultiStreamEditor))
				, ConcertClient(MoveTemp(ConcertClient))
				, ClientManager(ClientManager)
			{}
			
			virtual SHeaderRow::FColumn::FArguments CreateHeaderRowArgs() const override
			{
				return SHeaderRow::Column(AssignPropertyColumnId)
					.DefaultLabel(LOCTEXT("Owner.Label", "Assigned Client"))
					.ToolTipText(LOCTEXT("Owner.ToolTip", "Client that should replicate this property"))
					.FillSized(FMultiUserReplicationStyle::Get()->GetFloat(TEXT("AllClients.Property.OwnerColumnWidth")));
			}
			
			virtual TSharedRef<SWidget> GenerateColumnWidget(const FBuildArgs& InArgs) override
			{
				const TArray<FSoftObjectPath> DisplayedObjects = MultiStreamEditor.Get()->GetEditorBase().GetObjectsBeingPropertyEdited();
				return SNew(SAssignPropertyComboBox, MultiStreamEditor.Get().ToSharedRef(), ConcertClient, ClientManager)
					.DisplayedProperty(InArgs.RowItem.RowData.GetProperty())
					.EditedObjects(DisplayedObjects)
					.HighlightText(InArgs.HighlightText)
					.OnPropertyAssignmentChanged_Lambda([this]()
					{
						if (const TSharedPtr<IMultiReplicationStreamEditor> Editor = MultiStreamEditor.Get())
						{
							Editor->GetEditorBase().RequestPropertyColumnResort(AssignPropertyColumnId);
						}
					});
			}
			
			virtual void PopulateSearchString(const FPropertyTreeRowContext& InItem, TArray<FString>& InOutSearchStrings) const override
			{
				AssignPropertyColumnUtils::ForEachStreamAssignedTo(*MultiStreamEditor.Get(), InItem.RowData.GetProperty(),
					[this, &InOutSearchStrings](const TSharedRef<IEditableReplicationStreamModel>& Stream)
					{
						InOutSearchStrings.Add(AssignPropertyColumnUtils::GetClientDisplayText(*ConcertClient, ClientManager, *Stream));
					});
			}

			virtual bool CanBeSorted() const override { return true; }
			virtual bool IsLessThan(const FPropertyTreeRowContext& Left, const FPropertyTreeRowContext& Right) const override
			{
				const TArray<FSoftObjectPath> DisplayedObjects = MultiStreamEditor.Get()->GetEditorBase().GetObjectsBeingPropertyEdited();
				const TOptional<FString> LeftClientDisplayString = SAssignPropertyComboBox::GetDisplayString(ConcertClient, ClientManager, Left.RowData.GetProperty(), DisplayedObjects);
				const TOptional<FString> RightClientDisplayString = SAssignPropertyComboBox::GetDisplayString(ConcertClient, ClientManager, Right.RowData.GetProperty(), DisplayedObjects);
			
				if (LeftClientDisplayString && RightClientDisplayString)
				{
					return *LeftClientDisplayString < *RightClientDisplayString;
				}
				// Our rule: set < unset. This way unassigned appears last.
				return LeftClientDisplayString.IsSet() && !RightClientDisplayString.IsSet();
			}

		private:

			const TAttribute<TSharedPtr<IMultiReplicationStreamEditor>> MultiStreamEditor;
			const TSharedRef<IConcertClient> ConcertClient;
			FReplicationClientManager& ClientManager;
		};
		
		check(MultiStreamEditor.IsBound() || MultiStreamEditor.IsSet());
		return {
			TReplicationColumnDelegates<FPropertyTreeRowContext>::FCreateColumn::CreateLambda(
				[MultiStreamEditor = MoveTemp(MultiStreamEditor), ConcertClient = MoveTemp(ConcertClient), &ClientManager]()
				{
					return MakeShared<FPropertyColumn_AssignPropertyColumn>(MultiStreamEditor, ConcertClient, ClientManager);
				}),
			AssignPropertyColumnId,
			{ ColumnsSortPriority }
		};
	}
}

#undef LOCTEXT_NAMESPACE