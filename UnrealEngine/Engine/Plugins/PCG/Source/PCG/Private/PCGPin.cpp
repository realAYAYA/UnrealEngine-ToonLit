// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGPin.h"

#include "PCGCommon.h"
#include "PCGEdge.h"
#include "PCGNode.h"
#include "PCGSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGPin)

namespace PCGPin
{
	bool SortPinsAndRetrieveTypes(const UPCGPin* InPinA, const UPCGPin* InPinB, const UPCGPin*& OutUpstreamPin, const UPCGPin*& OutDownstreamPin, EPCGDataType& OutUpstreamTypes, EPCGDataType& OutDownstreamTypes)
	{
		check(InPinA && InPinB);
		const bool bPinAIsOutput = InPinA->IsOutputPin();

		if (!ensure(bPinAIsOutput != InPinB->IsOutputPin()))
		{
			// Cannot connect two pins of same polarity
			return false;
		}

		OutUpstreamPin = bPinAIsOutput ? InPinA : InPinB;
		OutDownstreamPin = bPinAIsOutput ? InPinB : InPinA;

		check(OutUpstreamPin && OutDownstreamPin);
		OutUpstreamTypes = OutUpstreamPin->GetCurrentTypes();
		OutDownstreamTypes = OutDownstreamPin->Properties.AllowedTypes;

		return true;
	}
}

#if WITH_EDITOR
namespace PCGPinPropertiesHelpers
{
	bool GetDefaultPinExtraIcon(const UPCGPin* InPin, FName& OutExtraIcon, FText& OutTooltip)
	{
		return InPin && PCGPinPropertiesHelpers::GetDefaultPinExtraIcon(InPin->Properties, OutExtraIcon, OutTooltip);
	}

	bool GetDefaultPinExtraIcon(const FPCGPinProperties& InPinProperties, FName& OutExtraIcon, FText& OutTooltip)
	{
		if (InPinProperties.Usage == EPCGPinUsage::Loop)
		{
			OutTooltip = NSLOCTEXT("PCGPins", "LoopTooltip", "Loop pin, data collection will be split to one data per execution.");
			OutExtraIcon = PCGPinConstants::Icons::LoopPinIcon;
			return true;
		}
		else if (InPinProperties.Usage == EPCGPinUsage::Feedback)
		{
			OutTooltip = NSLOCTEXT("PCGPins", "FeedbackTooltip", "Feedback pin, will daisy-chain results from graph entry or previous loop iteration.");
			OutExtraIcon = PCGPinConstants::Icons::FeedbackPinIcon;
			return true;
		}

		return false;
	}
}
#endif // WITH_EDITOR

FPCGPinProperties::FPCGPinProperties(const FName& InLabel, EPCGDataType InAllowedTypes, bool bInAllowMultipleConnections, bool bInAllowMultipleData, const FText& InTooltip)
	: Label(InLabel), AllowedTypes(InAllowedTypes), bAllowMultipleData(bInAllowMultipleData)
#if WITH_EDITORONLY_DATA
	, Tooltip(InTooltip)
#endif
{
	SetAllowMultipleConnections(bInAllowMultipleConnections);
}

void FPCGPinProperties::SetAllowMultipleConnections(bool bInAllowMultipleConnectons)
{
	if (bInAllowMultipleConnectons)
	{
		bAllowMultipleConnections = true;
		bAllowMultipleData = true;
	}
	else
	{
		bAllowMultipleConnections = false;
	}
}

bool FPCGPinProperties::operator==(const FPCGPinProperties& Other) const
{
	return Label == Other.Label &&
		AllowedTypes == Other.AllowedTypes &&
		bAllowMultipleConnections == Other.bAllowMultipleConnections &&
		bAllowMultipleData == Other.bAllowMultipleData &&
		Usage == Other.Usage &&
		PinStatus == Other.PinStatus &&
		bInvisiblePin == Other.bInvisiblePin;
}

void FPCGPinProperties::PostSerialize(const FArchive& Ar)
{
#if WITH_EDITOR
	if (Ar.IsLoading() && Ar.IsPersistent() && !Ar.HasAnyPortFlags(PPF_Duplicate | PPF_DuplicateForPIE))
	{
		if (bAdvancedPin_DEPRECATED)
		{
			PinStatus = EPCGPinStatus::Advanced;
		}

		bAdvancedPin_DEPRECATED = false;
	}
#endif // WITH_EDITOR
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

bool UPCGPin::AddEdgeTo(UPCGPin* OtherPin, TSet<UPCGNode*>* InTouchedNodes/*= nullptr*/)
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

	if (InTouchedNodes)
	{
		InTouchedNodes->Add(Node);
		InTouchedNodes->Add(OtherPin->Node);
	}

	return true;
}

bool UPCGPin::BreakEdgeTo(UPCGPin* OtherPin, TSet<UPCGNode*>* InTouchedNodes/*= nullptr*/)
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

			if (InTouchedNodes)
			{
				InTouchedNodes->Add(Node);
				InTouchedNodes->Add(OtherPin->Node);
			}

			return true;
		}
	}

	return false;
}

bool UPCGPin::BreakAllEdges(TSet<UPCGNode*>* InTouchedNodes/*= nullptr*/)
{
	bool bChanged = false;

	if (!Edges.IsEmpty())
	{
		if (InTouchedNodes)
		{
			InTouchedNodes->Add(Node);
		}

		Modify();
	}

	for (UPCGEdge* Edge : Edges)
	{
		if (UPCGPin* OtherPin = Edge->GetOtherPin(this))
		{
			OtherPin->Modify();
			ensure(OtherPin->Edges.Remove(Edge));
			bChanged = true;

			if (InTouchedNodes)
			{
				InTouchedNodes->Add(OtherPin->Node);
			}
		}
	}

	Edges.Reset();

	return bChanged;
}

bool UPCGPin::BreakAllIncompatibleEdges(TSet<UPCGNode*>* InTouchedNodes/*= nullptr*/)
{
	bool bChanged = false;
	bool bHasAValidEdge = false;

	for (int32 EdgeIndex = Edges.Num() - 1; EdgeIndex >= 0; --EdgeIndex)
	{
		UPCGEdge* Edge = Edges[EdgeIndex];
		UPCGPin* OtherPin = Edge->GetOtherPin(this);

		bool bRemoveEdge = !IsCompatible(OtherPin) || (!AllowsMultipleConnections() && bHasAValidEdge);

		if (bRemoveEdge)
		{
			Modify();
			Edges.RemoveAtSwap(EdgeIndex);

			if (InTouchedNodes)
			{
				InTouchedNodes->Add(Node);
			}

			if (OtherPin)
			{
				OtherPin->Modify();
				ensure(OtherPin->Edges.Remove(Edge));
				bChanged = true;

				if (InTouchedNodes)
				{
					InTouchedNodes->Add(OtherPin->Node);
				}
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

EPCGDataType UPCGPin::GetCurrentTypes() const
{
	check(Node);
	const UPCGSettings* Settings = Node->GetSettings();
	return Settings ? Settings->GetCurrentPinTypes(this) : Properties.AllowedTypes;
}

bool UPCGPin::IsCompatible(const UPCGPin* OtherPin) const
{
	if (!OtherPin)
	{
		return false;
	}

	const UPCGPin* UpstreamPin = nullptr;
	const UPCGPin* DownstreamPin = nullptr;
	EPCGDataType UpstreamTypes = EPCGDataType::None, DownstreamTypes = EPCGDataType::None;
	if (!PCGPin::SortPinsAndRetrieveTypes(this, OtherPin, UpstreamPin, DownstreamPin, UpstreamTypes, DownstreamTypes))
	{
		return false;
	}
	check(UpstreamPin && DownstreamPin);

	// Types missing
	if (UpstreamTypes == EPCGDataType::None || DownstreamTypes == EPCGDataType::None)
	{
		return false;
	}

	// Concrete can always be used as a composite - allow connections from concrete to composite
	const bool bUpstreamInConcrete = !(UpstreamTypes & ~EPCGDataType::Concrete);
	const bool bDownstreamIsSpatial = (DownstreamTypes == EPCGDataType::Spatial);
	if (bUpstreamInConcrete && bDownstreamIsSpatial)
	{
		return true;
	}

	// Anything spatial can collapse to point as long as the downstream pin does not support any other spatial types. For example, Point | Param is collapsible, but Point | Surface is not
	const bool bUpstreamIsSpatial = !(UpstreamTypes & ~EPCGDataType::Spatial);
	const bool bDownstreamIsPoint = (DownstreamTypes & EPCGDataType::Spatial) == EPCGDataType::Point;
	if (bUpstreamIsSpatial && bDownstreamIsPoint)
	{
		return true;
	}

	// Spline -> Surface conversion
	if (UpstreamTypes == EPCGDataType::Spline && DownstreamTypes == EPCGDataType::Surface)
	{
		return true;
	}

	// Otherwise allow if there is overlap. Don't detect wide -> narrow issues - conversion nodes deal with that
	return !!(UpstreamTypes & DownstreamTypes);
}

bool UPCGPin::AllowsMultipleConnections() const
{
	// Always allow multiple connection on output pin
	return IsOutputPin() || Properties.AllowsMultipleConnections();
}

bool UPCGPin::AllowsMultipleData() const
{
	return Properties.bAllowMultipleData;
}

bool UPCGPin::CanConnect(const UPCGPin* OtherPin) const
{
	return OtherPin && (Edges.IsEmpty() || AllowsMultipleConnections());
}

EPCGTypeConversion UPCGPin::GetRequiredTypeConversion(const UPCGPin* InOtherPin) const
{
	if (!InOtherPin)
	{
		return EPCGTypeConversion::Failed;
	}

	const UPCGPin* UpstreamPin = nullptr;
	const UPCGPin* DownstreamPin = nullptr;
	EPCGDataType UpstreamTypes = EPCGDataType::None;
	EPCGDataType DownstreamTypes = EPCGDataType::None;
	if (!PCGPin::SortPinsAndRetrieveTypes(this, InOtherPin, UpstreamPin, DownstreamPin, UpstreamTypes, DownstreamTypes))
	{
		return EPCGTypeConversion::Failed;
	}
	check(UpstreamPin && DownstreamPin);

	// Types same - trivial early out.
	if (DownstreamTypes == UpstreamTypes)
	{
		return EPCGTypeConversion::NoConversionRequired;
	}

	// Type missing, or types are the same - trivial early out.
	if (!DownstreamTypes || !UpstreamTypes)
	{
		return EPCGTypeConversion::Failed;
	}

	// Spatial -> Point - "To Point" conversion.
	// Anything spatial can collapse to point as long as the downstream pin does not support any other spatial types. For example, Point | Param is collapsible, but Point | Surface is not.
	const bool bUpstreamIsSpatial = !(UpstreamTypes & ~EPCGDataType::Spatial);
	const bool bUpstreamIsPoint = UpstreamTypes == EPCGDataType::Point;
	const bool bDownstreamIsSpatialAndPointOnly = (DownstreamTypes & EPCGDataType::Spatial) == EPCGDataType::Point;
	if (bUpstreamIsSpatial && bDownstreamIsSpatialAndPointOnly)
	{
		return bUpstreamIsPoint ? EPCGTypeConversion::NoConversionRequired : EPCGTypeConversion::CollapseToPoint;
	}

	// Spline -> Surface conversion
	if (UpstreamTypes == EPCGDataType::Spline && DownstreamTypes == EPCGDataType::Surface)
	{
		return EPCGTypeConversion::SplineToSurface;
	}

	// Any or Spatial -> Concrete - "Make Concrete" conversion. We decided to support Any as it is a superset of Concrete,
	// and some pins are Any until they dynamically narrow, and other pins may stay Any but still want to be supported.
	bool bUpstreamIsSpatialOrAny = (UpstreamTypes == EPCGDataType::Any) || (UpstreamTypes == EPCGDataType::Spatial);
	if (bUpstreamIsSpatialOrAny && DownstreamTypes == EPCGDataType::Concrete)
	{
		return EPCGTypeConversion::MakeConcrete;
	}

	// Requires filter if upstream type is broader than downstream type.
	if (!!(UpstreamTypes & (~DownstreamTypes)))
	{
		return EPCGTypeConversion::Filter;
	}

	return EPCGTypeConversion::NoConversionRequired;
}
