// Copyright Epic Games, Inc. All Rights Reserved.

#include "SingleClientColumns.h"

#include "IConcertClient.h"
#include "Replication/Authority/EAuthorityMutability.h"
#include "Replication/Editor/Model/ReplicatedObjectData.h"
#include "Replication/Editor/View/IReplicationStreamViewer.h"
#include "Replication/Util/GlobalAuthorityCache.h"
#include "Widgets/ActiveSession/Replication/Client/ClientUtils.h"
#include "Widgets/ActiveSession/Replication/Client/SWarningIcon.h"

#include "Algo/AllOf.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Views/SHeaderRow.h"

#define LOCTEXT_NAMESPACE "SingleClientColumns.ConflictWarning"

namespace UE::MultiUserClient::SingleClientColumns
{
	const FName ConflictWarningTopLevelObjectColumnId = TEXT("ConflictWarningTopLevelObjectColumn");
	const FName ConflictWarningPropertyColumnId = TEXT("ConflictWarningPropertyColumn");
	
	ConcertSharedSlate::FObjectColumnEntry ConflictWarningForObject(
		TSharedRef<IConcertClient> InClient,
		FGlobalAuthorityCache& InAuthorityCache,
		const FGuid& ClientId
		)
	{
		class FPropertyColumn_ConflictWarningForObject : public ConcertSharedSlate::IObjectTreeColumn
		{
		public:

			FPropertyColumn_ConflictWarningForObject(
				TSharedRef<IConcertClient> InClient,
				FGlobalAuthorityCache& InAuthorityCache,
				const FGuid& ClientId
				)
				: Client(MoveTemp(InClient))
				, AuthorityCache(InAuthorityCache)
				, ClientId(ClientId)
			{}
			
			virtual SHeaderRow::FColumn::FArguments CreateHeaderRowArgs() const override
			{
				return SHeaderRow::Column(ConflictWarningTopLevelObjectColumnId)
					.DefaultLabel(FText::GetEmpty())
					.FixedWidth(20.f);
			}
			
			virtual TSharedRef<SWidget> GenerateColumnWidget(const FBuildArgs& InArgs) override
			{
				const auto GetVisibility = [this, ObjectPath = InArgs.RowItem.RowData.GetObjectPath()]()
				{
					const EAuthorityMutability TakeAuthorityResult = AuthorityCache.CanClientTakeAuthorityAfterSubmission(ObjectPath, ClientId);
					return TakeAuthorityResult == EAuthorityMutability::Conflict
						? EVisibility::Visible
						: EVisibility::Collapsed;
				};
				const auto GetToolTip = [this, ObjectPath = InArgs.RowItem.RowData.GetObjectPath()]()
				{
					TSet<FString> ClientNames;
					const EAuthorityMutability TakeAuthorityResult = AuthorityCache.CanClientTakeAuthorityAfterSubmission(
						ObjectPath,
						ClientId,
						[this, &ClientNames](const FGuid& InClientId, const FConcertPropertyChain&)
						{
							ClientNames.Add(ClientUtils::GetClientDisplayName(*Client, InClientId));
							return EBreakBehavior::Continue;
						});
						
					return TakeAuthorityResult != EAuthorityMutability::Conflict
						? FText::GetEmpty()
						: FText::Format(
							LOCTEXT("Subobject.WarningFmt", "{0} {1}|plural(one=is,other=are) replicating some of the assigned properties. This change will not be sent to the server."),
							FText::FromString(FString::Join(ClientNames, TEXT(", "))),
							ClientNames.Num()
						);
				};
					
				return SNew(SWarningIcon)
					.Visibility_Lambda(GetVisibility)
					.ToolTipText_Lambda(GetToolTip);
			}

		private:

			const TSharedRef<IConcertClient> Client;
			FGlobalAuthorityCache& AuthorityCache;
			const FGuid ClientId;
		};

		return {
			ConcertSharedSlate::TReplicationColumnDelegates<ConcertSharedSlate::FObjectTreeRowContext>::FCreateColumn::CreateLambda(
				[InClient = MoveTemp(InClient), &InAuthorityCache, ClientId]()
				{
					return MakeShared<FPropertyColumn_ConflictWarningForObject>(InClient, InAuthorityCache, ClientId);
				}),
			ConflictWarningTopLevelObjectColumnId,
			{ static_cast<int32>(ETopLevelObjectColumnOrder::ConflictWarning) }
		};
	}

	ConcertSharedSlate::FPropertyColumnEntry ConflictWarningForProperty(
		TSharedRef<IConcertClient> InClient,
		TAttribute<const ConcertSharedSlate::IReplicationStreamViewer*> Viewer,
		FGlobalAuthorityCache& InAuthorityCache,
		const FGuid& ClientId
		)
	{
		class FPropertyColumn_ConflictWarningForProperty : public ConcertSharedSlate::IPropertyTreeColumn
		{
		public:

			FPropertyColumn_ConflictWarningForProperty(
				TSharedRef<IConcertClient> InClient,
				TAttribute<const ConcertSharedSlate::IReplicationStreamViewer*> Viewer,
				FGlobalAuthorityCache& InAuthorityCache,
				const FGuid& ClientId
				)
				: Client(MoveTemp(InClient))
				, Viewer((MoveTemp(Viewer)))
				, AuthorityCache(InAuthorityCache)
				, ClientId(ClientId)
			{}
			
			virtual SHeaderRow::FColumn::FArguments CreateHeaderRowArgs() const override
			{
				return SHeaderRow::Column(ConflictWarningPropertyColumnId)
					.DefaultLabel(FText::GetEmpty())
					.FixedWidth(20.f);
			}
			
			virtual TSharedRef<SWidget> GenerateColumnWidget(const FBuildArgs& InArgs) override
			{
				const auto GetVisibility = [this, Property = InArgs.RowItem.RowData.GetProperty()]()
				{
					const bool bCanAddProperty = Algo::AllOf(Viewer.Get()->GetObjectsBeingPropertyEdited(),
						[this, Property](const FSoftObjectPath& ObjectPath)
						{
							return AuthorityCache.CanClientAddProperty(ObjectPath, ClientId, Property);
						});
					return bCanAddProperty
						? EVisibility::Collapsed
						: EVisibility::Visible;
				};
				const auto GetToolTip = [this, Property = InArgs.RowItem.RowData.GetProperty()]()
				{
					TSet<FString> Names;
					for (const FSoftObjectPath& ObjectPath: Viewer.Get()->GetObjectsBeingPropertyEdited())
					{
						const TOptional<FGuid> Owner = AuthorityCache.GetClientWithAuthorityOverProperty(ObjectPath, Property);
						if (Owner || *Owner != ClientId)
						{
							Names.Add(ClientUtils::GetClientDisplayName(*Client, *Owner));
						}
					}
						
					return Names.IsEmpty()
						? FText::GetEmpty()
						: FText::Format(
							LOCTEXT("Property.WarningFmt", "Replicated by {0}. This property will not be submitted to the server."),
							FText::FromString(FString::Join(Names, TEXT(", ")))
							);
				};
					
				return SNew(SWarningIcon)
					.Visibility_Lambda(GetVisibility)
					.ToolTipText_Lambda(GetToolTip);
			}

		private:

			const TSharedRef<IConcertClient> Client;
			const TAttribute<const ConcertSharedSlate::IReplicationStreamViewer*> Viewer;
			FGlobalAuthorityCache& AuthorityCache;
			const FGuid ClientId;
		};

		return {
			ConcertSharedSlate::TReplicationColumnDelegates<ConcertSharedSlate::FPropertyTreeRowContext>::FCreateColumn::CreateLambda(
				[InClient = MoveTemp(InClient), Viewer = MoveTemp(Viewer), &InAuthorityCache, ClientId]()
				{
					return MakeShared<FPropertyColumn_ConflictWarningForProperty>(InClient, Viewer, InAuthorityCache, ClientId);
				}),
			ConflictWarningPropertyColumnId,
			{ static_cast<int32>(EPropertyColumnOrder::ConflictWarning) }
		};
	}
}

#undef LOCTEXT_NAMESPACE