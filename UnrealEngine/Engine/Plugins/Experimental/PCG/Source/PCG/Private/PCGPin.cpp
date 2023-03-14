// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGPin.h"
#include "PCGEdge.h"
#include "PCGNode.h"

FPCGPinProperties::FPCGPinProperties(const FName& InLabel, EPCGDataType InAllowedTypes, bool bInAllowMultipleConnections)
	: Label(InLabel), AllowedTypes(InAllowedTypes), bAllowMultipleConnections(bInAllowMultipleConnections)
{}

bool FPCGPinProperties::operator==(const FPCGPinProperties& Other) const
{
	return Label == Other.Label && AllowedTypes == Other.AllowedTypes && bAllowMultipleConnections == Other.bAllowMultipleConnections;
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

	Modify();
	OtherPin->Modify();

	UPCGEdge* NewEdge = Edges.Add_GetRef(NewObject<UPCGEdge>(this));
	OtherPin->Edges.Add(NewEdge);

	NewEdge->InputPin = this;
	NewEdge->OutputPin = OtherPin;

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

		bool bRemoveEdge = !IsCompatible(OtherPin) || (!Properties.bAllowMultipleConnections && bHasAValidEdge);

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
	return OtherPin && !!(Properties.AllowedTypes & OtherPin->Properties.AllowedTypes);
}

bool UPCGPin::CanConnect(const UPCGPin* OtherPin) const
{
	return OtherPin && (Properties.bAllowMultipleConnections || Edges.IsEmpty());
}