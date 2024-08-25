// Copyright Epic Games, Inc. All Rights Reserved.

#include "SingleClientColumns.h"

#include "IConcertClient.h"
#include "MultiUserReplicationStyle.h"
#include "Replication/Editor/View/IReplicationStreamViewer.h"
#include "Replication/Editor/View/Column/ReplicationColumnsUtils.h"
#include "Replication/Util/GlobalAuthorityCache.h"
#include "SOwnerClientList.h"
#include "Widgets/ActiveSession/Replication/Client/ClientUtils.h"

#define LOCTEXT_NAMESPACE "SingleClientColumns.Owner"

namespace UE::MultiUserClient::SingleClientColumns
{
	const FName OwnerOfSubobjectColumnId = TEXT("OwnerOfSubobjectColumn");
	const FName OwnerOfPropertyColumnId = TEXT("OwnerOfPropertyColumn");

	namespace Private::Shared
	{
		static void PopulateSearchTerms(const TSharedRef<IConcertClient>& InClient, const TArray<FGuid>& Clients, TArray<FString>& InOutSearchStrings)
		{
			for (const FGuid& ClientId : Clients)
			{
				InOutSearchStrings.Add(ClientUtils::GetClientDisplayName(*InClient, ClientId));
			}
		}
	}
	
	ConcertSharedSlate::FObjectColumnEntry OwnerOfObject(
		TSharedRef<IConcertClient> InClient,
		FGlobalAuthorityCache& InAuthorityCache
		)
	{
		class FObjectColum_OwnerOfObject : public ConcertSharedSlate::IObjectTreeColumn
		{
		public:

			FObjectColum_OwnerOfObject(
				TSharedRef<IConcertClient> Client,
				FGlobalAuthorityCache& AuthorityCache
				)
				: Client(MoveTemp(Client))
				, AuthorityCache(AuthorityCache)
			{}
			
			virtual SHeaderRow::FColumn::FArguments CreateHeaderRowArgs() const override
			{
				return SHeaderRow::Column(OwnerOfSubobjectColumnId)
					.DefaultLabel(LOCTEXT("Owner.Label", "Assigned Clients"))
					.ToolTipText(LOCTEXT("Owner.ToolTip", "Clients that have registered properties for an object"))
					.FillSized(FMultiUserReplicationStyle::Get()->GetFloat(TEXT("SingleClient.Object.OwnerColumnWidth")));
			}
			
			virtual TSharedRef<SWidget> GenerateColumnWidget(const FBuildArgs& InArgs) override
			{
				return SNew(SOwnerClientList, Client, AuthorityCache)
					.GetClientList_Lambda([this, ObjectPath = InArgs.RowItem.RowData.GetObjectPath()](const FGlobalAuthorityCache&)
					{
						return AuthorityCache.GetClientsWithAuthorityOverObject(ObjectPath);
					})
					.HighlightText_Lambda([HighlightText = InArgs.HighlightText](){ return *HighlightText.Get(); });
			}
			
			virtual void PopulateSearchString(const ConcertSharedSlate::FObjectTreeRowContext& InItem, TArray<FString>& InOutSearchStrings) const override
			{
				Private::Shared::PopulateSearchTerms(
					Client,
					AuthorityCache.GetClientsWithAuthorityOverObject(InItem.RowData.GetObjectPath()),
					InOutSearchStrings
					);
			}

		private:
			
			const TSharedRef<IConcertClient> Client;
			FGlobalAuthorityCache& AuthorityCache;
		};

		return {
			ConcertSharedSlate::TReplicationColumnDelegates<ConcertSharedSlate::FObjectTreeRowContext>::FCreateColumn::CreateLambda(
				[InClient = MoveTemp(InClient), &InAuthorityCache]()
				{
					return MakeShared<FObjectColum_OwnerOfObject>(InClient, InAuthorityCache);
				}),
			OwnerOfSubobjectColumnId,
			{ static_cast<int32>(ETopLevelObjectColumnOrder::Owner) }
		};
	}
	
	namespace Private::Property
	{
		static TArray<FGuid> GetPropertyOwners(
			const FGlobalAuthorityCache& InAuthorityCache,
			const ConcertSharedSlate::IReplicationStreamViewer& InViewer,
			const FConcertPropertyChain& Property
			)
		{
			TArray<FGuid> Result;
			const TArray<FSoftObjectPath> SelectedObjects = InViewer.GetObjectsBeingPropertyEdited();
			for (const FSoftObjectPath& SelectedObject : SelectedObjects)
			{
				const TOptional<FGuid> Owner = InAuthorityCache.GetClientWithAuthorityOverProperty(SelectedObject, Property);
				if (Owner)
				{
					Result.AddUnique(*Owner);
				}
			}
			return Result;
		}
	}

	ConcertSharedSlate::FPropertyColumnEntry OwnerOfProperty(
		TSharedRef<IConcertClient> InClient,
		FGlobalAuthorityCache& InAuthorityCache,
		TAttribute<const ConcertSharedSlate::IReplicationStreamViewer*> InViewer
		)
	{
		class FPropertyColumn_OwnerOfProperty : public ConcertSharedSlate::IPropertyTreeColumn
		{
		public:
			
			FPropertyColumn_OwnerOfProperty(
				TSharedRef<IConcertClient> Client,
				FGlobalAuthorityCache& AuthorityCache,
				TAttribute<const ConcertSharedSlate::IReplicationStreamViewer*> Viewer
				)
				: Client(MoveTemp(Client))
				, AuthorityCache(AuthorityCache)
				, Viewer(MoveTemp(Viewer))
			{}
			
			virtual SHeaderRow::FColumn::FArguments CreateHeaderRowArgs() const override
			{
				return SHeaderRow::Column(OwnerOfPropertyColumnId)
					.DefaultLabel(LOCTEXT("Property.Owner", "Owner"))
					.FillSized(FMultiUserReplicationStyle::Get()->GetFloat(TEXT("SingleClient.Property.OwnerColumnWidth")));
			}
			
			virtual TSharedRef<SWidget> GenerateColumnWidget(const FBuildArgs& InArgs) override
			{
				return SNew(SOwnerClientList, Client, AuthorityCache)
					.GetClientList_Lambda([this, Property = InArgs.RowItem.RowData.GetProperty()](const FGlobalAuthorityCache&)
					{
						return Private::Property::GetPropertyOwners(AuthorityCache, *Viewer.Get(), Property);
					})
					.HighlightText_Lambda([HighlightText = InArgs.HighlightText](){ return *HighlightText.Get(); });
			}
			
			virtual void PopulateSearchString(const ConcertSharedSlate::FPropertyTreeRowContext& InItem, TArray<FString>& InOutSearchStrings) const override
			{
				Private::Shared::PopulateSearchTerms(
					Client,
					Private::Property::GetPropertyOwners(AuthorityCache, *Viewer.Get(), InItem.RowData.GetProperty()),
					InOutSearchStrings
					);
			}

		private:
			
			const TSharedRef<IConcertClient> Client;
			FGlobalAuthorityCache& AuthorityCache;
			const TAttribute<const ConcertSharedSlate::IReplicationStreamViewer*> Viewer;
		};

		return {
			ConcertSharedSlate::TReplicationColumnDelegates<ConcertSharedSlate::FPropertyTreeRowContext>::FCreateColumn::CreateLambda(
				[InClient = MoveTemp(InClient), &InAuthorityCache, InViewer = MoveTemp(InViewer)]()
				{
					return MakeShared<FPropertyColumn_OwnerOfProperty>(InClient, InAuthorityCache, InViewer);
				}),
			OwnerOfPropertyColumnId,
			{ static_cast<int32>(EPropertyColumnOrder::Owner) }
		};
	}
}

#undef LOCTEXT_NAMESPACE