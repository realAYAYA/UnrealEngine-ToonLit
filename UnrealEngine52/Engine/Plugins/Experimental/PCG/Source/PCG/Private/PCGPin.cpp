// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGPin.h"
#include "PCGEdge.h"
#include "PCGNode.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGPin)

FPCGPinProperties::FPCGPinProperties(const FName& InLabel, EPCGDataType InAllowedTypes, bool bInAllowMultipleConnections, bool bInAllowMultipleData, const FText& InTooltip)
	: Label(InLabel), AllowedTypes(InAllowedTypes), bAllowMultipleData(bInAllowMultipleData), bAllowMultipleConnections(bInAllowMultipleConnections)
#if WITH_EDITORONLY_DATA
	, Tooltip(InTooltip)
#endif
{}

bool FPCGPinProperties::operator==(const FPCGPinProperties& Other) const
{
	return Label == Other.Label &&
		AllowedTypes == Other.AllowedTypes &&
		bAllowMultipleConnections == Other.bAllowMultipleConnections &&
		bAllowMultipleData == Other.bAllowMultipleData &&
		bAdvancedPin == Other.bAdvancedPin;
}

UPCGPin::UPCGPin(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetFlags(RF_Transactional);
}

void UPCGPin::PostLoad()
{
	Super::PostLoad();

	if (Label_DEPRECATED != NAME_None)
	{
		Properties = FPCGPinProperties(Label_DEPRECATED);
		Label_DEPRECATED = NAME_None;
	}
}

FText UPCGPin::GetTooltip() const
{
#if WITH_EDITOR
	return Properties.Tooltip;
#else
	return FText::GetEmpty();
#endif
}

void UPCGPin::SetTooltip(const FText& InTooltip)
{
#if WITH_EDITOR
	Properties.Tooltip = InTooltip;
#endif
}

bool UPCGPin::AddEdgeTo(UPCGPin* OtherPin)
{
	if (!OtherPin)
	{
		return false;
	}

	for (UPCGEdge* Edge : Edges)
	{
		if (Edge->GetOtherPin(this) == OtherPin)
		{
			return false;
		}
	}

	// This pin is upstream if the pin is an output pin
	const bool bThisPinIsUpstream = IsOutputPin();
	const bool bOtherPinIsUpstream = OtherPin->IsOutputPin();

	// Pins should not both be upstream or both be downstream..
	if (!ensure(bThisPinIsUpstream != bOtherPinIsUpstream))
	{
		return false;
	}

	Modify();
	OtherPin->Modify();

	UPCGEdge* NewEdge = Edges.Add_GetRef(NewObject<UPCGEdge>(this));
	OtherPin->Edges.Add(NewEdge);
	
	NewEdge->Modify();
	NewEdge->InputPin = bThisPinIsUpstream ? this : OtherPin;
	NewEdge->OutputPin = bThisPinIsUpstream ? OtherPin : this;

	return true;
}

bool UPCGPin::BreakEdgeTo(UPCGPin* OtherPin)
{
	if (!OtherPin)
	{
		return false;
	}

	for (UPCGEdge* Edge : Edges)
	{
		if (Edge->GetOtherPin(this) == OtherPin)
		{
			Modify();
			OtherPin->Modify();

			ensure(OtherPin->Edges.Remove(Edge));
			Edges.Remove(Edge);
			return true;
		}
	}

	return false;
}

bool UPCGPin::BreakAllEdges()
{
	bool bChanged = false;

	if (!Edges.IsEmpty())
	{
		Modify();
	}

	for (UPCGEdge* Edge : Edges)
	{
		if (UPCGPin* OtherPin = Edge->GetOtherPin(this))
		{
			OtherPin->Modify();
			ensure(OtherPin->Edges.Remove(Edge));
			bChanged = true;
		}
	}

	Edges.Reset();

	return bChanged;
}

bool UPCGPin::BreakAllIncompatibleEdges()
{
	bool bChanged = false;
	bool bHasAValidEdge = false;

	for (int32 EdgeIndex = Edges.Num() - 1; EdgeIndex >= 0; --EdgeIndex)
	{
		UPCGEdge* Edge = Edges[EdgeIndex];
		UPCGPin* OtherPin = Edge->GetOtherPin(this);

		bool bRemoveEdge = !IsCompatible(OtherPin) || (!AllowMultipleConnections() && bHasAValidEdge);

		if (bRemoveEdge)
		{
			Modify();
			Edges.RemoveAtSwap(EdgeIndex);

			if (OtherPin)
			{
				OtherPin->Modify();
				ensure(OtherPin->Edges.Remove(Edge));
				bChanged = true;
			}
		}
		else
		{
			bHasAValidEdge = true;
		}
	}

	return bChanged;
}

bool UPCGPin::IsConnected() const
{
	for (const UPCGEdge* Edge : Edges)
	{
		if (Edge->IsValid())
		{
			return true;
		}
	}

	return false;
}

bool UPCGPin::IsOutputPin() const
{
	check(Node);
	return Node->GetOutputPin(Properties.Label) == this;
}

int32 UPCGPin::EdgeCount() const
{
	int32 EdgeNum = 0;
	for (const UPCGEdge* Edge : Edges)
	{
		if (Edge->IsValid())
		{
			++EdgeNum;
		}
	}

	return EdgeNum;
}

bool UPCGPin::IsCompatible(const UPCGPin* OtherPin) const
{
	if (!OtherPin)
	{
		return false;
	}

	const bool bThisPinOutput = IsOutputPin();
	if (!ensure(bThisPinOutput != OtherPin->IsOutputPin()))
	{
		// Cannot connect two pins of same polarity
		return false;
	}

	// Sort pins
	const UPCGPin* UpstreamPin = bThisPinOutput ? this : OtherPin;
	const UPCGPin* DownstreamPin = bThisPinOutput ? OtherPin : this;

	// Concrete can always be used as a composite - allow connections from concrete to composite
	const bool bUpstreamConcrete = !!(UpstreamPin->Properties.AllowedTypes & EPCGDataType::Concrete);
	const bool bDownstreamSpatial = !!(DownstreamPin->Properties.AllowedTypes & EPCGDataType::Spatial);
	if (bUpstreamConcrete && bDownstreamSpatial)
	{
		return true;
	}

	// Catch case when a composite type is being connected to a concrete type - that can require collapse
	const bool bUpstreamSpatial = !!(UpstreamPin->Properties.AllowedTypes & EPCGDataType::Spatial);
	const bool bDownstreamConcrete = !!(DownstreamPin->Properties.AllowedTypes & EPCGDataType::Concrete);
	if (bUpstreamSpatial && bDownstreamConcrete)
	{
		// This will trigger a collapse, but let it slide for now.
		// TODO in the future we should inject a conversion node.
		return true;
	}

	return !!(Properties.AllowedTypes & OtherPin->Properties.AllowedTypes);
}

bool UPCGPin::AllowMultipleConnections() const
{
	// Always allow multiple connection on output pin
	return IsOutputPin() || Properties.bAllowMultipleConnections;
}

bool UPCGPin::CanConnect(const UPCGPin* OtherPin) const
{
	return OtherPin && (Edges.IsEmpty() || AllowMultipleConnections());
}
