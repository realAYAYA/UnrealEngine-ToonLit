// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Replication/Editor/View/Column/IPropertyTreeColumn.h"
#include "Replication/Editor/View/Column/IObjectTreeColumn.h"

#include "Delegates/Delegate.h"
#include "Misc/EnumClassFlags.h"

class IConcertClient;

namespace UE::MultiUserClient
{
	class FReassignObjectPropertiesLogic;
	class FReplicationClientManager;
}

namespace UE::ConcertSharedSlate
{
	class IObjectHierarchyModel;
	class IReplicationStreamModel;
	class IMultiReplicationStreamEditor;
	class IReplicationStreamViewer;
}

namespace UE::MultiUserClient::MultiStreamColumns
{
	const extern FName ReplicationToggleColumnId;
	const extern FName ReassignOwnershipColumnId;
	const extern FName AssignPropertyColumnId;
	
	/* @see ETopLevelColumnOrder and EReplicationPropertyColumnOrder */
	enum class EColumnSortOrder
	{
		ReplicationToggle = 0,
		AssignPropertyColumn = 30,
		ReassignOwnership = 40
	};

	/**
	 * Toggles replication for all clients assigned to the object (and optionally all children).
	 * 
	 * @param ConcertClient Used to look up client names
	 * @param ObjectHierarchyModelAttribute Used to get child objects
	 * @param ClientManager Used to access all clients for toggling authority
	 * @param ColumnsSortPriority The order relative to the other columns
	 * 
	 * @return A checkbox for controlling the authority of the object in the row
	 */
	ConcertSharedSlate::FObjectColumnEntry ReplicationToggle(
		TSharedRef<IConcertClient> ConcertClient,
		TAttribute<ConcertSharedSlate::IObjectHierarchyModel*> ObjectHierarchyModelAttribute,
		FReplicationClientManager& ClientManager,
		const int32 ColumnsSortPriority = static_cast<int32>(EColumnSortOrder::ReplicationToggle)
		);

	/**
	 * A combo box which displays all current owners for an object and allows bulk reassigning properties.
	 *
	 * @param ConcertClient Used to look up client names
	 * @param MultiStreamModelAttribute Used for requesting resorting the column
	 * @param ObjectHierarchyModelAttribute Used to get child objects
	 * @param ReassignmentLogic Performs the act of reassigning
	 * @param ClientManager Used to access all clients for display in the combo box drop-down
	 * @param ColumnsSortPriority The order relative to the other columns
	 */
	ConcertSharedSlate::FObjectColumnEntry ReassignOwnership(
		TSharedRef<IConcertClient> ConcertClient,
		TAttribute<TSharedPtr<ConcertSharedSlate::IMultiReplicationStreamEditor>> MultiStreamModelAttribute,
		TAttribute<ConcertSharedSlate::IObjectHierarchyModel*> ObjectHierarchyModelAttribute,
		FReassignObjectPropertiesLogic& ReassignmentLogic,
		const FReplicationClientManager& ClientManager,
		int32 ColumnsSortPriority = static_cast<int32>(EColumnSortOrder::ReassignOwnership)
		);
	
	/**
	 * Creates a property column which assigns the property to the stream selected in the combo box.
	 * 
	 * @param MultiStreamEditor Used to determine the selected objects.
	 * @param ConcertClient Used to look up client names
	 * @param ClientManager Used to map streams back to client display info
	 * @param ColumnsSortPriority The order relative to the other columns
	 * 
	 * @return A column that spawns a combo box for assigning properties
	 */
	ConcertSharedSlate::FPropertyColumnEntry AssignPropertyColumn(
		TAttribute<TSharedPtr<ConcertSharedSlate::IMultiReplicationStreamEditor>> MultiStreamEditor,
		TSharedRef<IConcertClient> ConcertClient,
		FReplicationClientManager& ClientManager,
		const int32 ColumnsSortPriority = static_cast<int32>(EColumnSortOrder::AssignPropertyColumn)
		);
}
