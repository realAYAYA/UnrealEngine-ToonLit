// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGReroute.h"

#include "PCGContext.h"
#include "PCGEdge.h"
#include "PCGModule.h"
#include "PCGNode.h"
#include "PCGPin.h"

UPCGRerouteSettings::UPCGRerouteSettings()
{
#if WITH_EDITORONLY_DATA
	bExposeToLibrary = false;

	// Reroutes don't support disabling or debugging.
	bDisplayDebuggingProperties = false;
	DebugSettings.bDisplayProperties = false;
	bEnabled = true;
	bDebug = false;
#endif
}

TArray<FPCGPinProperties> UPCGRerouteSettings::InputPinProperties() const
{
	FPCGPinProperties PinProperties;
	PinProperties.Label = PCGPinConstants::DefaultInputLabel;
	PinProperties.SetAllowMultipleConnections(false);
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

TArray<FPCGPinProperties> UPCGNamedRerouteDeclarationSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	// Default visible pin
	PinProperties.Emplace(
		PCGPinConstants::DefaultOutputLabel, 
		EPCGDataType::Any, 
		/*bAllowMultipleConnections=*/true, 
		/*bAllowMultipleData=*/true);

	FPCGPinProperties& InvisiblePin = PinProperties.Emplace_GetRef(
		PCGNamedRerouteConstants::InvisiblePinLabel,
		EPCGDataType::Any,
		/*bAllowMultipleConnections=*/true,
		/*bAllowMultipleData=*/true);
	InvisiblePin.bInvisiblePin = true;
	
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGNamedRerouteUsageSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	if (ensure(PinProperties.Num() == 1))
	{
		PinProperties[0].bInvisiblePin = true;
	}

	return PinProperties;
}

EPCGDataType UPCGNamedRerouteUsageSettings::GetCurrentPinTypes(const UPCGPin* InPin) const
{
	// Defer to declaration if possible
	return Declaration ? Declaration->GetCurrentPinTypes(InPin) : Super::GetCurrentPinTypes(InPin);
}

bool FPCGRerouteElement::ExecuteInternal(FPCGContext* Context) const
{
	check(Context);

	// Reroute elements are culled during graph compilation unless they have no inbound edge.
	// In such as case, this is a good place to log an error for user to deal with.
	PCGE_LOG(Error, GraphAndLog, NSLOCTEXT("PCGRerouteSettings", "DetachedReroute", "Reroute is not linked to anything. Reconnect to recreate to fix the error."));
	
	Context->OutputData = Context->InputData;
	for (FPCGTaggedData& Output : Context->OutputData.TaggedData)
	{
		Output.Pin = PCGPinConstants::DefaultOutputLabel;
	}

	return true;
}
