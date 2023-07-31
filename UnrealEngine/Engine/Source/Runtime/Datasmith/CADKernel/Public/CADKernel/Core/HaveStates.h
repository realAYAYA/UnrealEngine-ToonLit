// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CADKernel/Core/Types.h"
#include "CADKernel/Core/CADKernelArchive.h"

enum class EHaveStates : uint16
{
	None = 0x0000u,  // No flags.

	IsApplyCriteria = 0x0001u,  // used for FTopologicalFace, FEdge
	IsRemoved = 0x0001u,  // used for FThinZone2D

	IsMeshed = 0x0002u,  // used for FTopologicalFace, FEdge
	IsInner = 0x0002u,  // used for FEdgeSegment, FShell, 
	IsFirstSide = 0x0002u,  // used for FThinZoneSide
	FirstSideClosed = 0x0002u,  // used for FThinZone2D

	IsBackOriented = 0x0004u,  // used for FTopologicalFace
	ThinPeak = 0x0004u,  // used for FEdge 
	SecondSideClosed = 0x0004u,  // used for FThinZone2D

	ThinZone = 0x0008u,  // used for FTopologicalFace, FEdge, FEdgeSegment  

	Degenerated = 0x0010u,  // used for FEdge, FGrid

	IsDeleted = 0x0020u,  // used for all class
	IsVirtuallyMeshed = 0x0040u,  // used for FEdge

	HasMarker1 = 0x1000u,  // used for all class
	HasMarker2 = 0x2000u,  // used for all class
	HasMarker3 = 0x4000u,  // used for all class
	IsProcess4 = 0x8000u,  // used for all class
	AllMarkers = 0xF000u,  // used for all class

	All = 0xFFu
};

ENUM_CLASS_FLAGS(EHaveStates);

namespace UE::CADKernel
{
class CADKERNEL_API FHaveStates
{
protected:
	mutable EHaveStates States;

public:

	FHaveStates()
		: States(EHaveStates::None)
	{};

	void Serialize(FCADKernelArchive& Ar)
	{
		Ar << States;
	}

	void ResetElementStatus()
	{
		States = EHaveStates::None;
	}

	bool HasMarker1() const
	{
		return ((States & EHaveStates::HasMarker1) == EHaveStates::HasMarker1);
	}

	bool HasMarker2() const
	{
		return ((States & EHaveStates::HasMarker2) == EHaveStates::HasMarker2);
	}

	bool HasMarker3() const
	{
		return ((States & EHaveStates::HasMarker3) == EHaveStates::HasMarker3);
	}

	void SetMarker1() const
	{
		States |= EHaveStates::HasMarker1;
	}

	void SetMarker2() const
	{
		States |= EHaveStates::HasMarker2;
	}

	void SetMarker3() const
	{
		States |= EHaveStates::HasMarker3;
	}

	void ResetMarker1() const
	{
		States &= ~EHaveStates::HasMarker1;
	}

	void ResetMarker2() const
	{
		States &= ~EHaveStates::HasMarker2;
	}

	void ResetMarker3() const
	{
		States &= ~EHaveStates::HasMarker3;
	}

	void ResetMarkers() const
	{
		States &= ~EHaveStates::AllMarkers;
	}

	bool IsDeleted() const
	{
		return ((States & EHaveStates::IsDeleted) == EHaveStates::IsDeleted);
	}

	void SetDeleted() const
	{
		States |= EHaveStates::IsDeleted;
	}

	void ResetDeleted() const
	{
		States &= ~EHaveStates::IsDeleted;
	}

	virtual bool IsDegenerated() const
	{
		return ((States & EHaveStates::Degenerated) == EHaveStates::Degenerated);
	}

	virtual void SetAsDegenerated() const
	{
		States |= EHaveStates::Degenerated;
	}

	virtual void ResetDegenerated() const
	{
		States &= ~EHaveStates::Degenerated;
	}

};
}

