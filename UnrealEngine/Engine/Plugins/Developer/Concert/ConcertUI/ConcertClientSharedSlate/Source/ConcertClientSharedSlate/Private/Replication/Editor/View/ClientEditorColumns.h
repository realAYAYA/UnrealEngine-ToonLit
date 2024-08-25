// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Replication/ClientReplicationWidgetDelegates.h"
#include "Replication/Editor/View/Column/ReplicationColumnsUtils.h"
#include "Replication/Editor/View/Column/IPropertyTreeColumn.h"

#include "Templates/SharedPointer.h"

namespace UE::ConcertSharedSlate
{
	class IEditableReplicationStreamModel;
	class IReplicationStreamViewer;
	class IReplicationStreamModel;
	class IObjectHierarchyModel;
}

namespace UE::ConcertClientSharedSlate::ReplicationColumns::Property
{
	extern const FName ReplicatesColumnId;
	
	enum class EReplicationPropertyColumnOrder : int32
	{
		/** The checkbox in SDefaultReplicationStreamEditor determining whether the property is in the selection*/
		ReplicatesCheckbox = 0,
	};
	
	/**
	 * A checkbox that is placed at the beginning of every property.
	 * Checking & unchecking adds & removes the property to the selected objects' property mapping, respectively.
	 *
	 * @param Viewer Used to determine which objects are currently selected (and being edited by the checkbox)
	 * @param Model Used to actually edit the object to properties mapping
	 * @param ExtendPropertiesDelegate Delegate to determine additional properties to add when the property checkbox is checked.
	 * @param IsEnabledDelegate Determines whether the checkbox is enabled
	 * @param DisabledToolTipText Tooltip to display when IsEnabledDelegate returns false
	 * @param ColumnWidth Width to use for the column
	 * @param Priority Determines position of this columns relative to the others
	 */
	ConcertSharedSlate::FPropertyColumnEntry ReplicatesColumns(
		TAttribute<ConcertSharedSlate::IReplicationStreamViewer*> Viewer, // TODO DP Columns: Refactor this
		TWeakPtr<ConcertSharedSlate::IEditableReplicationStreamModel> Model,
		FExtendProperties ExtendPropertiesDelegate = {},
		ConcertSharedSlate::TCheckboxColumnDelegates<ConcertSharedSlate::FPropertyTreeRowContext>::FIsEnabled IsEnabledDelegate = {},
		TAttribute<FText> DisabledToolTipText = {},
		const float ColumnWidth = 20.f,
		const int32 Priority = static_cast<int32>(EReplicationPropertyColumnOrder::ReplicatesCheckbox)
		);
	
	/**
	 * Goes through all selected objects, checks whether the property is checked on it or not, and returns a checkbox state.
	 * The state is checked or unchecked if all selected properties have the property enabled or disabled, respectively, and undetermined otherwise, i.e. if it is mixed.
	 *
	 * This is used by ReplicatesColumns.
	 */
	ECheckBoxState GetPropertyCheckboxStateBasedOnSelection(
		const FConcertPropertyChain& Property,
		TConstArrayView<FSoftObjectPath> Selection,
		const ConcertSharedSlate::IReplicationStreamModel& Model
		);
}
