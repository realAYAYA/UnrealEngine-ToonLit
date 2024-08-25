// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Replication/Editor/View/Column/IObjectTreeColumn.h"
#include "Replication/Editor/View/Column/IPropertyTreeColumn.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class IConcertClient;

namespace UE::ConcertSharedSlate
{
	class IReplicationStreamViewer;
	class IReplicationStreamModel;
}

namespace UE::MultiUserClient
{
	class FAuthorityChangeTracker;
	class FGlobalAuthorityCache;
	class IClientAuthoritySynchronizer;
	class ISubmissionWorkflow;
}

namespace UE::MultiUserClient::SingleClientColumns
{
	/** @see UE::ConcertSharedSlate::ReplicationColumns::TopLevel::ETopLevelColumnOrder */
	enum class ETopLevelObjectColumnOrder
	{
		ToggleAuthority = 0,
		ConflictWarning = 5,
		Owner = 40
	};
	/** @see UE::ConcertSharedSlate::ReplicationColumns::Property::EReplicationPropertyColumnOrder */
	enum class EPropertyColumnOrder
	{
		ConflictWarning = 5,
		Owner = 50
	};


	/********** Toggle authority **********/
	extern const FName ToggleObjectAuthorityColumnId;

	/**
	 * Checkbox placed in the subobject view
	 * It gives / removes authority for the object it is placed next to.
	 * 
	 * @param ChangeTracker Used to determine the checkbox state and change authority.
	 * @param SubmissionWorkflow
	 * @return Column that can be placed in the table
	 */
	ConcertSharedSlate::FObjectColumnEntry ToggleObjectAuthority(
		FAuthorityChangeTracker& ChangeTracker,
		ISubmissionWorkflow& SubmissionWorkflow
		);

	
	/********** Owner **********/
	extern const FName OwnerOfSubobjectColumnId;
	extern const FName OwnerOfPropertyColumnId;

	/**
	 * Displays the owner of a subobject.
	 * @param InClient The local Concert client used to look up other client display info
	 * @param InAuthorityCache Used to determine which client owns the property
	 * @return Column that can be placed in the table
	 */
	ConcertSharedSlate::FObjectColumnEntry OwnerOfObject(
		TSharedRef<IConcertClient> InClient,
		FGlobalAuthorityCache& InAuthorityCache
		);
	
	/**
	 * Displays the owner of a property.
	 * @param InClient The local Concert client used to look up other client display info
	 * @param InAuthorityCache Used to determine which client owns the property
	 * @param InViewerAttribute Used to determine which objects the property box is displaying
	 * @return Column that can be placed in the table
	 */
	ConcertSharedSlate::FPropertyColumnEntry OwnerOfProperty(
		TSharedRef<IConcertClient> InClient,
		FGlobalAuthorityCache& InAuthorityCache,
		TAttribute<const ConcertSharedSlate::IReplicationStreamViewer*> InViewerAttribute
		);

	
	/********** Conflict warning **********/
	extern const FName ConflictWarningTopLevelObjectColumnId;
	extern const FName ConflictWarningPropertyColumnId;
	
	/**
	 * Displays a warning symbol next to the checkbox if checking the checkbox would cause an authority conflict when submitted.
	 * 
	 * @param InClient The local Concert client used to look up other client display info
	 * @param InAuthorityCache Used to determine which client owns the object
	 * @param ClientId ID of the client for which the icon is being created
	 * 
	 * @return Column that can be placed in the table
	 */
	ConcertSharedSlate::FObjectColumnEntry ConflictWarningForObject(
		TSharedRef<IConcertClient> InClient,
		FGlobalAuthorityCache& InAuthorityCache,
		const FGuid& ClientId
		);

	/**
	 * Displays a warning symbol next to the checkbox if checking the checkbox would cause an authority conflict when submitted.
	 * 
	 * @param InClient The local Concert client used to look up other client display info
	 * @param InViewer Used to determine which objects the property box is displaying
	 * @param InAuthorityCache Used to determine which client owns the property
	 * @param ClientId ID of the client for which to check whether the property can be added to the stream
	 * 
	 * @return Column that can be placed in the table
	 */
	ConcertSharedSlate::FPropertyColumnEntry ConflictWarningForProperty(
		TSharedRef<IConcertClient> InClient,
		TAttribute<const ConcertSharedSlate::IReplicationStreamViewer*> InViewer,
		FGlobalAuthorityCache& InAuthorityCache,
		const FGuid& ClientId
		);
}
