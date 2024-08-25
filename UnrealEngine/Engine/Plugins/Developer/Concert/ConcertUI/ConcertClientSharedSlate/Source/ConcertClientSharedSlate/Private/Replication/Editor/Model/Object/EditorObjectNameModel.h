// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Replication/Editor/Model/Object/IObjectNameModel.h"

namespace UE::ConcertClientSharedSlate
{
	/** Name model that uses editor data for determining display names: actors use their labels, components ask USubobjectDataSubsystem.  */
	class FEditorObjectNameModel : public ConcertSharedSlate::IObjectNameModel
	{
	public:

		//~ Begin IObjectNameModel Interface
		virtual FText GetObjectDisplayName(const FSoftObjectPath& ObjectPath) const override;
		//~ End IObjectNameModel Interface
	};
}

