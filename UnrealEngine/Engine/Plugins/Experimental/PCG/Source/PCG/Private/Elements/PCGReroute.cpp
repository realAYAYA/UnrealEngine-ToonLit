// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGReroute.h"

#include "PCGContext.h"
#include "PCGEdge.h"
#include "PCGNode.h"
#include "PCGPin.h"

UPCGRerouteSettings::UPCGRerouteSettings()
{
#if WITH_EDITORONLY_DATA
	bExposeToLibrary = false;
#endif
}

EPCGDataType UPCGRerouteSettings::GetCurrentPinTypes(const UPCGPin* InPin) const
{
	// All pins have same type
	EPCGDataType PinTypes = EPCGDataType::Any;
	if (UPCGNode* PCGNode = Cast<UPCGNode>(GetOuter()))
	{
		if (UPCGPin* InputPin = PCGNode->GetInputPin(PCGPinConstants::DefaultInputLabel))
		{
			if (InputPin->EdgeCount() > 0)
			{
				if (UPCGEdge* Edge = InputPin->Edges[0])
				{
					if (const UPCGPin* OtherOutputPin = Edge->GetOtherPin(InputPin))
					{
						PinTypes = OtherOutputPin->GetCurrentTypes();
					}
				}
			}
		}
	}

	return PinTypes;
}

TArray<FPCGPinProperties> UPCGRerouteSettings::InputPinProperties() const
{
	FPCGPinProperties PinProperties;
	PinProperties.Label = PCGPinConstants::DefaultInputLabel;
	PinProperties.bAllowMultipleConnections = false;
	PinProperties.AllowedTypes = EPCGDataType::Any;

	return { PinProperties };
}

TArray<FPCGPinProperties> UPCGRerouteSettings::OutputPinProperties() const
{
	FPCGPinProperties PinProperties;
	PinProperties.Label = PCGPinConstants::DefaultOutputLabel;
	PinProperties.AllowedTypes = EPCGDataType::Any;

	return { PinProperties };
}

FPCGElementPtr UPCGRerouteSettings::CreateElement() const
{
	return MakeShared<FPCGRerouteElement>();
}

bool FPCGRerouteElement::ExecuteInternal(FPCGContext* Context) const
{
	Context->OutputData = Context->InputData;
	return true;
}
