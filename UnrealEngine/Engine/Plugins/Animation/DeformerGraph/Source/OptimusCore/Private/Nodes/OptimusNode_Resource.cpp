// Copyright Epic Games, Inc. All Rights Reserved.

#include "Nodes/OptimusNode_Resource.h"

#include "OptimusNodePin.h"
#include "OptimusResourceDescription.h"
#include "DataInterfaces/OptimusDataInterfaceRawBuffer.h"


int32 UOptimusNode_Resource::GetDataFunctionIndexFromPin(const UOptimusNodePin* InPin) const
{
	if (!InPin || InPin->GetParentPin() != nullptr)
	{
		return INDEX_NONE;
	}
	if (!ensure(GetPins().Contains(InPin)))
	{
		return INDEX_NONE;
	}

	return InPin->GetDirection() == EOptimusNodePinDirection::Output 
		? UOptimusRawBufferDataInterface::GetReadValueInputIndex() 
		: UOptimusRawBufferDataInterface::GetWriteValueOutputIndex(WriteType);
}


FName UOptimusNode_Resource::GetResourcePinName(int32 InPinIndex) const
{
	if (const UOptimusResourceDescription* Res = GetResourceDescription())
	{
		if (ensure(InPinIndex >= 0 && InPinIndex < 2))
		{
			FString Suffix = InPinIndex == 0 ? TEXT("Set") : TEXT("Get");
			return FName(Suffix + Res->ResourceName.ToString());
		}
	}

	return {};
}


void UOptimusNode_Resource::ConstructNode()
{
	if (const UOptimusResourceDescription* Res  = GetResourceDescription())
	{
		AddPinDirect(
			GetResourcePinName(0),
			EOptimusNodePinDirection::Input,
			Res->DataDomain,
			Res->DataType);

		const FName GetterName("Get");

		AddPinDirect(
			GetResourcePinName(1),
			EOptimusNodePinDirection::Output,
			Res->DataDomain,
			Res->DataType);
	}
}
