// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IObjectTreeColumn.h"
#include "IPropertyTreeColumn.h"
#include "ReplicationColumnInfo.h"

#include "Templates/SharedPointer.h"

namespace UE::ConcertSharedSlate
{
	struct FObjectTreeRowContext;
	struct FPropertyTreeRowContext;
	class IEditableReplicationStreamModel;
	class IReplicationStreamViewer;
	class IReplicationStreamModel;
	class IObjectNameModel;
}

namespace UE::ConcertSharedSlate::ReplicationColumns::TopLevel
{
	CONCERTSHAREDSLATE_API extern const FName IconColumnId;
	CONCERTSHAREDSLATE_API extern const FName LabelColumnId;
	CONCERTSHAREDSLATE_API extern const FName TypeColumnId;

	enum class ETopLevelColumnOrder : int32
	{
		/** Label of the object */
		Label = 20,
		/** Class of the object */
		Type = 30,
	};

	CONCERTSHAREDSLATE_API FObjectColumnEntry LabelColumn(TSharedRef<IReplicationStreamModel> Model, IObjectNameModel* OptionalNameModel = nullptr);
	CONCERTSHAREDSLATE_API FObjectColumnEntry TypeColumn(TSharedRef<IReplicationStreamModel> Model);
}

namespace UE::ConcertSharedSlate::ReplicationColumns::Property
{
	CONCERTSHAREDSLATE_API extern const FName LabelColumnId;
	CONCERTSHAREDSLATE_API extern const FName TypeColumnId;
	
	enum class EReplicationPropertyColumnOrder : int32
	{
		/** Label of the property */
		Label = 10,
		/** Type of the property */
		Type = 20
	};
	
	CONCERTSHAREDSLATE_API FPropertyColumnEntry LabelColumn();
	CONCERTSHAREDSLATE_API FPropertyColumnEntry TypeColumn();
}
