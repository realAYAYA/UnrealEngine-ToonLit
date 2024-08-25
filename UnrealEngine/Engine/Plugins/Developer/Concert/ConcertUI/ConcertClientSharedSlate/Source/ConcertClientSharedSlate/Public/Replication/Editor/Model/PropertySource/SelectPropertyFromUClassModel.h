// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Replication/Editor/Model/Property/IPropertySelectionSourceModel.h"

#include "Templates/SharedPointer.h"

struct FSoftClassPath;

namespace UE::ConcertClientSharedSlate
{
	class FConcertSyncCoreReplicatedPropertySource;
	
	/**
	 * Decides which properties can be added to IEditableReplicationStreamModel.
	 * The allowed properties are those returned by UE::ConcertSyncCore::ForEachReplicatableProperty.
	 */
	class CONCERTCLIENTSHAREDSLATE_API FSelectPropertyFromUClassModel : public ConcertSharedSlate::IPropertySelectionSourceModel
	{
	public:

		FSelectPropertyFromUClassModel();

		//~ Begin IPropertySelectionSourceModel Interface
		virtual TSharedRef<ConcertSharedSlate::IPropertySourceModel> GetPropertySource(const FSoftClassPath& Class) const override;
		//~ End IPropertySelectionSourceModel Interface

	private:

		TSharedRef<FConcertSyncCoreReplicatedPropertySource> UClassIteratorSource;
	};
}

