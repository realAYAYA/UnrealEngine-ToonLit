// Copyright Epic Games, Inc. All Rights Reserved.

#include "Nodes/OptimusNode_GetResource.h"

#include "OptimusResourceDescription.h"
#include "OptimusNodePin.h"
#include "DataInterfaces/OptimusDataInterfaceRawBuffer.h"


int32 UOptimusNode_GetResource::GetDataFunctionIndexFromPin(const UOptimusNodePin* InPin) const
{
	if (!InPin || InPin->GetParentPin() != nullptr)
	{
		return INDEX_NONE;
	}
	if (!ensure(GetPins().Contains(InPin)))
	{
		return INDEX_NONE;
	}

	return UOptimusRawBufferDataInterface::GetReadValueInputIndex();
}

void UOptimusNode_GetResource::ConstructNode()
{
	if (const UOptimusResourceDescription *Res = GetResourceDescription())
	{
		// FIXME: Define context.
		AddPinDirect(
			GetResourcePinName(0),
			EOptimusNodePinDirection::Output,
			Res->DataDomain,
			Res->DataType);
	}
}
