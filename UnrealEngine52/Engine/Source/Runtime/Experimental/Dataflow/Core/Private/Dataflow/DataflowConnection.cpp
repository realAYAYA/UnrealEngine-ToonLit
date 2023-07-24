// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowConnection.h"

#include "Dataflow/DataflowNodeParameters.h"
#include "Dataflow/DataflowNode.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DataflowConnection)

FDataflowConnection::FDataflowConnection(Dataflow::FPin::EDirection InDirection, FName InType, FName InName, FDataflowNode* InOwningNode, FProperty* InProperty, FGuid InGuid)
	: Direction(InDirection)
	, Type(InType)
	, Name(InName)
	, OwningNode(InOwningNode)
	, Property(InProperty)
	, Guid(InGuid)
{}

int32 FDataflowConnection::GetOffset() const
{
	if (ensure(Property != nullptr))
	{
		return Property->GetOffset_ForInternal();
	}
	return INDEX_NONE;
}


