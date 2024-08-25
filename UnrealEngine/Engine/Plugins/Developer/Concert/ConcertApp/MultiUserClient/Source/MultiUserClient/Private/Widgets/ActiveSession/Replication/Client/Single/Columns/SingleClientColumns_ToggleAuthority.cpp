// Copyright Epic Games, Inc. All Rights Reserved.

#include "SingleClientColumns.h"

#include "IConcertClient.h"
#include "SOwnerClientList.h"
#include "Replication/Authority/AuthorityChangeTracker.h"
#include "Replication/Authority/EAuthorityMutability.h"
#include "Replication/Authority/IClientAuthoritySynchronizer.h"
#include "Replication/Editor/Model/IReplicationStreamModel.h"
#include "Replication/Editor/View/IReplicationStreamViewer.h"
#include "Replication/Editor/View/Column/ReplicationColumnsUtils.h"
#include "Replication/Editor/View/Column/IObjectTreeColumn.h"
#include "Replication/Editor/View/Column/IPropertyTreeColumn.h"
#include "Replication/Submission/ISubmissionWorkflow.h"
#include "Replication/Util/GlobalAuthorityCache.h"
#include "Widgets/ClientName/SClientName.h"

#define LOCTEXT_NAMESPACE "SingleClientColumns.ToggleAuthority"

namespace UE::MultiUserClient::SingleClientColumns
{
	const FName ToggleObjectAuthorityColumnId = TEXT("ToggleTopLevelAuthorityColumn");

	ConcertSharedSlate::FObjectColumnEntry ToggleObjectAuthority(
		FAuthorityChangeTracker& ChangeTracker,
		ISubmissionWorkflow& SubmissionWorkflow
		)
	{
		using namespace ConcertSharedSlate;
		using FColumnDelegates = TCheckboxColumnDelegates<FObjectTreeRowContext>;
		return MakeCheckboxColumn<FObjectTreeRowContext>(
			ToggleObjectAuthorityColumnId,
				FColumnDelegates(
					FColumnDelegates::FGetColumnCheckboxState::CreateLambda(
					[&ChangeTracker](const FObjectTreeRowContext& ObjectData)
					{
						const bool bHasAuthority = ChangeTracker.GetAuthorityStateAfterApplied(ObjectData.RowData.GetObjectPath());
						return bHasAuthority ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
					}),
					FColumnDelegates::FOnColumnCheckboxChanged::CreateLambda(
					[&ChangeTracker](bool bIsChecked, const FObjectTreeRowContext& ObjectData)
					{
						ChangeTracker.SetAuthorityIfAllowed({ ObjectData.RowData.GetObjectPath() }, bIsChecked);
					}),
					FColumnDelegates::FGetToolTipText::CreateLambda([&ChangeTracker, &SubmissionWorkflow](const FObjectTreeRowContext& ObjectData)
					{
						if (!CanEverSubmit(SubmissionWorkflow.GetUploadability()))
						{
							return LOCTEXT("ToggleAuthority.ToolTip.NotSupported", "This client cannot be remotely edited.");
						}
						
						switch (ChangeTracker.GetChangeAuthorityMutability(ObjectData.RowData.GetObjectPath()))
						{
						case EAuthorityMutability::Allowed: return LOCTEXT("ToggleAuthority.ToolTip.Allowed", "Whether to replicate this object.");
						case EAuthorityMutability::NotApplicable: return LOCTEXT("ToggleAuthority.ToolTip.NotApplicable", "Assign properties to replicate first.");
						case EAuthorityMutability::Conflict: return LOCTEXT("ToggleAuthority.ToolTip.Conflict", "Another client is replicating this property already.");
						default: checkNoEntry(); return FText::GetEmpty();
						}
					}),
					FColumnDelegates::FIsEnabled::CreateLambda([&ChangeTracker](const FObjectTreeRowContext& ObjectData)
					{
						return ChangeTracker.CanSetAuthorityFor(ObjectData.RowData.GetObjectPath());
					})
				),
			FText::GetEmpty(),
			static_cast<int32>(ETopLevelObjectColumnOrder::ToggleAuthority)
		);
	}
}

#undef LOCTEXT_NAMESPACE