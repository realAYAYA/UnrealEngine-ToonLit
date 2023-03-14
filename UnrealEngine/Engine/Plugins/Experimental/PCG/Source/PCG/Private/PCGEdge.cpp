// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGEdge.h"
#include "PCGNode.h"

bool UPCGEdge::IsValid() const
{
	return InputPin.Get() && OutputPin.Get();
}

UPCGPin* UPCGEdge::GetOtherPin(const UPCGPin* Pin)
{
	check(Pin == InputPin || Pin == OutputPin);
	return Pin == InputPin ? OutputPin : InputPin;
}

const UPCGPin* UPCGEdge::GetOtherPin(const UPCGPin* Pin) const
{
	check(Pin == InputPin || Pin == OutputPin);
	return Pin == InputPin ? OutputPin : InputPin;
}