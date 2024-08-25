// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Replication/Editor/Model/IReplicationStreamModel.h"

namespace UE::ConcertSharedSlate
{
	class IPropertySelectionSourceModel;
	
	/**
	 * This model is passed tp SObjectToPropertyView by SObjectToPropertyEditor.
	 *
	 * The goal here is to mock some of the functions so the UI displays more than is in the model for an easier editing
	 * experience:
	 * - The root object outliner will only displays AActors.
	 * - The property view will display the properties reported by an IPropertySelectionSourceModel (which is usually ALL properties of the object class).
	 * 
	 * SObjectToPropertyEditor injects column checkboxes on property rows which handle adding the properties to the real,
	 * underlying model.
	 * @see UE::ConcertSharedSlate::ReplicationPropertyColumns::ReplicatesColumns.
	 */
	class FFakeObjectToPropertiesEditorModel : public IReplicationStreamModel
	{
	public:

		FFakeObjectToPropertiesEditorModel(
			TSharedRef<IReplicationStreamModel> RealModel,
			TSharedRef<IPropertySelectionSourceModel> PropertySelectionSource
			)
			: RealModel(MoveTemp(RealModel))
			, PropertySelectionSource(MoveTemp(PropertySelectionSource))
		{}

		//~ Begin IReplicationStreamModel Interface
		// Technically these functions should be also be wrapped but the SObjectToPropertyView does not use them so let's not for now.
		virtual bool ContainsObjects(const TSet<FSoftObjectPath>& Objects) const override { return RealModel->ContainsObjects(Objects); }
		virtual bool ContainsProperties(const FSoftObjectPath& Object, const TSet<FConcertPropertyChain>& Properties) const override { return RealModel->ContainsProperties(Object, Properties); }
		
		virtual FSoftClassPath GetObjectClass(const FSoftObjectPath& Object) const override;
		virtual bool ForEachReplicatedObject(TFunctionRef<EBreakBehavior(const FSoftObjectPath& Object)> Delegate) const override;
		virtual bool ForEachProperty(const FSoftObjectPath& ObjectPath, TFunctionRef<EBreakBehavior(const FConcertPropertyChain& Property)> Delegate) const override;
		//~ End IReplicationStreamModel Interface

	private:

		/** The real model which is used to implement all the other functions. */
		const TSharedRef<IReplicationStreamModel> RealModel;

		/** Determines the properties that can be selected. */
		const TSharedRef<IPropertySelectionSourceModel> PropertySelectionSource;

		/** Returns all objects that must be listed in the top-outliner section of the replication view. */
		bool IterateObjects(TFunctionRef<EBreakBehavior(const FSoftObjectPath& Object)> Delegate) const;
		/** Iterates all properties on the given object's path. */
		void IterateDisplayedProperties(const FSoftObjectPath& ObjectPath, TFunctionRef<EBreakBehavior(const FConcertPropertyChain& Property)> Delegate) const;
	};
}

