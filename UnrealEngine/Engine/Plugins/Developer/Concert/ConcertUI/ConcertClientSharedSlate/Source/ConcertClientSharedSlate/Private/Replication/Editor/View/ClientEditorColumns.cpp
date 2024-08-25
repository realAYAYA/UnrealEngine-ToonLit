// Copyright Epic Games, Inc. All Rights Reserved.

#include "Replication/Editor/View/ClientEditorColumns.h"

#include "Replication/Editor/Model/IEditableReplicationStreamModel.h"
#include "Replication/Editor/Model/ReplicatedPropertyData.h"
#include "Replication/Editor/View/Column/ReplicationColumnsUtils.h"
#include "Replication/Editor/View/IReplicationStreamViewer.h"
#include "Replication/ObjectUtils.h"

#include "Internationalization/Internationalization.h"

#define LOCTEXT_NAMESPACE "ReplicationPropertyColumns"

namespace UE::ConcertClientSharedSlate::ReplicationColumns::Property
{
	const FName ReplicatesColumnId = TEXT("ReplicatedColumn");
	
	namespace Private
	{
		static ECheckBoxState OnGetPropertyCheckboxState(
			const FConcertPropertyChain& PropertyChain,
			const ConcertSharedSlate::IReplicationStreamViewer& Viewer,
			const ConcertSharedSlate::IReplicationStreamModel& Model
			)
		{
			const TArray<FSoftObjectPath> SelectedObjectPaths = Viewer.GetObjectsBeingPropertyEdited();
			return GetPropertyCheckboxStateBasedOnSelection(PropertyChain, SelectedObjectPaths, Model);
		}

		static void OnPropertyCheckboxChanged(
			bool bIsChecked,
			const FConcertPropertyChain& PropertyChain,
			const ConcertSharedSlate::IReplicationStreamViewer& Viewer,
			ConcertSharedSlate::IEditableReplicationStreamModel& Model,
			const FExtendProperties& ExtendPropertiesDelegate
			)
		{
			const TArray PropertyAsArray{ PropertyChain };
			const TArray<FSoftObjectPath> SelectedObjects = Viewer.GetObjectsBeingPropertyEdited();

			if (bIsChecked)
			{
				// We cannot proceed if any of the selected objects cannot be loaded because we cannot obtain its class
				for (const FSoftObjectPath& Path : SelectedObjects)
				{
					UObject* Object = Path.ResolveObject();
					if (!Object)
					{
						return;
					}
				}
				
				for (const FSoftObjectPath& Path : Viewer.GetObjectsBeingPropertyEdited())
				{
					UObject* Object = Path.ResolveObject();
					
					if (!Model.ContainsObjects({ Path }))
					{
						// FYI: This should always be a subobject because actors are in the view beforehand (this checkbox is created in response to clicking the actor)
						Model.AddObjects({ Object });
					}

					TArray<FConcertPropertyChain> PropertiesToAdd = PropertyAsArray;
					ExtendPropertiesDelegate.ExecuteIfBound(Path, PropertiesToAdd);
					Model.AddProperties(Path, PropertiesToAdd);
				}
			}
			else
			{
				for (const FSoftObjectPath& Path : Viewer.GetObjectsBeingPropertyEdited())
				{
					Model.RemoveProperties(Path, PropertyAsArray);

					// The actor was already in the view before this checkbox was created (the user must click an object for this checkbox to be created)
					// So in short, this checkbox should only remove the objects it has previously added.
					// This checkbox should remove subobjects because they were added by this checkbox before but not actors.
					// Also FYI: actors cause their subobjects to be automatically displayed in the top view so UX wise it would be weird if you uncheck
					// a property checkbox in the bottom view and then in the top view the object disappears.
					if (ConcertSharedSlate::ObjectUtils::IsActor(Path) && !Model.HasAnyPropertyAssigned(Path))
					{
						Model.RemoveObjects({ Path });
					}
				}
			}
		}
	}
	
	ConcertSharedSlate::FPropertyColumnEntry ReplicatesColumns(
		TAttribute<ConcertSharedSlate::IReplicationStreamViewer*> Viewer,
		TWeakPtr<ConcertSharedSlate::IEditableReplicationStreamModel> Model,
		FExtendProperties ExtendPropertiesDelegate,
		ConcertSharedSlate::TCheckboxColumnDelegates<ConcertSharedSlate::FPropertyTreeRowContext>::FIsEnabled IsEnabledDelegate,
		TAttribute<FText> DisabledToolTipText,
		const float ColumnWidth,
		const int32 Priority
		)
	{
		using namespace ConcertSharedSlate;
		using FPropertyColumnDelegates = TCheckboxColumnDelegates<FPropertyTreeRowContext>;
		return MakeCheckboxColumn<FPropertyTreeRowContext>(
			ReplicatesColumnId,
			FPropertyColumnDelegates(
				FPropertyColumnDelegates::FGetColumnCheckboxState::CreateLambda(
				[Viewer, Model](const FPropertyTreeRowContext& Data)
				{
					const IReplicationStreamViewer* ViewerPin = Viewer.Get();
					const TSharedPtr<IEditableReplicationStreamModel> ModelPin = Model.Pin();
					return ensure(ViewerPin && ModelPin) ? Private::OnGetPropertyCheckboxState(Data.RowData.GetProperty(), *ViewerPin, *ModelPin) : ECheckBoxState::Undetermined;
				}),
				FPropertyColumnDelegates::FOnColumnCheckboxChanged::CreateLambda(
				[Viewer, Model, ExtendPropertiesDelegate = MoveTemp(ExtendPropertiesDelegate)](bool bIsChecked, const FPropertyTreeRowContext& Data)
				{
					const IReplicationStreamViewer* ViewerPin = Viewer.Get();
					const TSharedPtr<IEditableReplicationStreamModel> ModelPin = Model.Pin();
					if (ensure(ViewerPin && ModelPin))
					{
						Private::OnPropertyCheckboxChanged(bIsChecked, Data.RowData.GetProperty(), *ViewerPin, *ModelPin, ExtendPropertiesDelegate);
					}
				}),
				FPropertyColumnDelegates::FGetToolTipText::CreateLambda(
				[IsEnabledDelegate, DisabledToolTipText = MoveTemp(DisabledToolTipText)](const FPropertyTreeRowContext& Data)
				{
					const bool bIsDisabled = IsEnabledDelegate.IsBound() && !IsEnabledDelegate.Execute(Data);
					const bool bCanCall = DisabledToolTipText.IsBound() || DisabledToolTipText.IsSet();
					return bIsDisabled
						? bCanCall ? DisabledToolTipText.Get() : FText::GetEmpty()
						: LOCTEXT("Replicates.ToolTip", "Select whether this property should be replicated");
				}),
				IsEnabledDelegate
				),
			FText::GetEmpty(),
			Priority,
			ColumnWidth
		);
	}

	ECheckBoxState GetPropertyCheckboxStateBasedOnSelection(
		const FConcertPropertyChain& Property,
		TConstArrayView<FSoftObjectPath> Selection,
		const ConcertSharedSlate::IReplicationStreamModel& Model)
	{
		ECheckBoxState CheckBoxState = ECheckBoxState::Undetermined;
		for (const FSoftObjectPath& SelectedObject : Selection)
		{
			const bool bContainsProperty = Model.ContainsProperties(SelectedObject, { Property });
			const ECheckBoxState StateForThisObject = bContainsProperty
				? ECheckBoxState::Checked
				: ECheckBoxState::Unchecked;
			// The first object?
			if (CheckBoxState == ECheckBoxState::Undetermined)
			{
				CheckBoxState = StateForThisObject;
			}
			else if (CheckBoxState != StateForThisObject)
			{
				// Two boxes do not have the same value
				return ECheckBoxState::Undetermined;
			}
		}
		return CheckBoxState;
	}
}

#undef LOCTEXT_NAMESPACE