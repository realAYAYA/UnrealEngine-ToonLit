// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CADKernel/Core/Types.h"
#include "CADKernel/Mesh/Meshers/IsoTriangulator/IsoNode.h"
#include "CADKernel/UI/Display.h"

namespace UE::CADKernel
{

enum class ESegmentType : uint8
{
	Unknown = 0,
	Loop = 1,
	ThinZone = 2,
	IsoU,
	IsoV,
	LoopToLoop, // IsBoundaryToInner use that xxxToBoundary are bigger than BoundaryToBoundary
	IsoLoopToLoop,
	InnerToLoopU,
	InnerToLoopV,
	InnerToLoop,
};

enum class EIsoSegmentStates : uint8
{
	None = 0x00u,	// No flags.

	CandidateFlag = 0x01u,

	LeftCycle = 0x02u,
	RightCycle = 0x04u,

	Degenerate = 0x08u,

	LeftTriangle = 0x10u,
	RightTriangle = 0x20u,
	SegmentComplete = 0x30u,

	StatusIsoU = 0x40u,
	StatusIsoV = 0x80u,

	Delete = 0xFFu,
	All = 0xFFu
};

ENUM_CLASS_FLAGS(EIsoSegmentStates);


class FGrid;

class FIsoSegment
{
protected:
	FIsoNode* FirstNode;
	FIsoNode* SecondNode;
	ESegmentType Type;
	EIsoSegmentStates States;

public:

	FIsoSegment()
		: FirstNode(nullptr)
		, SecondNode(nullptr)
		, Type(ESegmentType::Unknown)
		, States(EIsoSegmentStates::None)
	{
	}

	void Init(FIsoNode& InFirstNode, FIsoNode& InSecondNode, const ESegmentType InType)
	{
		States = EIsoSegmentStates::None;
		FirstNode = &InFirstNode;
		SecondNode = &InSecondNode;

		Type = InType;
		switch (Type)
		{
		case ESegmentType::IsoU:
			InFirstNode.SetLinkedToIso(EIsoLink::IsoUNext);
			InSecondNode.SetLinkedToIso(EIsoLink::IsoUPrevious);
			break;
		case ESegmentType::IsoV:
			InFirstNode.SetLinkedToIso(EIsoLink::IsoVNext);
			InSecondNode.SetLinkedToIso(EIsoLink::IsoVPrevious);
			break;
		}
	}

	void ConnectToNode()
	{
		TFunction<bool()> CheckExistingSegment = [&]()
		{
			if (IsItAlreadyDefined(FirstNode, SecondNode))
			{
				//Wait();
				return false;
			}
			return true;
		};

		ensureCADKernel(CheckExistingSegment());

		FirstNode->ConnectSegment(*this);
		SecondNode->ConnectSegment(*this);
	}

	static bool	IsItAlreadyDefined(const FIsoNode* StartNode, const FIsoNode* EndNode)
	{
		for (const FIsoSegment* Segment : StartNode->GetConnectedSegments())
		{
			const FIsoNode& OtherNode = (StartNode == &Segment->GetFirstNode()) ? Segment->GetSecondNode() : Segment->GetFirstNode();
			if (&OtherNode == EndNode)
			{
				return true;
			}
		}
		return false;
	}

	void Clean()
	{
		if (FirstNode)
		{
			FirstNode->DisconnectSegment(*this);
		}
		if (SecondNode)
		{
			SecondNode->DisconnectSegment(*this);
		}
		FirstNode = nullptr;
		SecondNode = nullptr;
		States = EIsoSegmentStates::Delete;
	}

	bool IsDelete() const
	{
		return States == EIsoSegmentStates::Delete;
	}

	void SetSelected()
	{
		States &= ~EIsoSegmentStates::CandidateFlag;
	}

	void SetCandidate()
	{
		States |= EIsoSegmentStates::CandidateFlag;
	}

	bool IsACandidate() const // [0 Pi/2]
	{
		return (States & EIsoSegmentStates::CandidateFlag) == EIsoSegmentStates::CandidateFlag;
	}

	bool HasCycleOnLeft()
	{
		return (States & EIsoSegmentStates::LeftCycle) == EIsoSegmentStates::LeftCycle;
	}

	void SetHaveCycleOnLeft()
	{
		States |= EIsoSegmentStates::LeftCycle;
	}

	bool HasCycleOnRight()
	{
		return (States & EIsoSegmentStates::RightCycle) == EIsoSegmentStates::RightCycle;
	}

	void SetHaveCycleOnRight()
	{
		States |= EIsoSegmentStates::RightCycle;
	}

	bool HasTriangleOnLeft()
	{
		return (States & EIsoSegmentStates::LeftTriangle) == EIsoSegmentStates::LeftTriangle;
	}

	void SetHasTriangleOnLeft()
	{
		States |= EIsoSegmentStates::LeftTriangle;
	}

	bool HasTriangleOnRight()
	{
		return (States & EIsoSegmentStates::RightTriangle) == EIsoSegmentStates::RightTriangle;
	}

	bool HasTriangleOnRightAndLeft()
	{
		return (States & EIsoSegmentStates::RightTriangle) == EIsoSegmentStates::RightTriangle && (States & EIsoSegmentStates::LeftTriangle) == EIsoSegmentStates::LeftTriangle;
	}

	void SetHasTriangleOnRight()
	{
		States |= EIsoSegmentStates::RightTriangle;
	}

	void ResetHasTriangle()
	{
		States &= ~EIsoSegmentStates::SegmentComplete;
	}

	void SetAsIsoU()
	{
		States |= EIsoSegmentStates::StatusIsoU;
	}

	bool IsIsoU() const
	{
		return (States & EIsoSegmentStates::StatusIsoU) == EIsoSegmentStates::StatusIsoU;
	}

	void SetAsIsoV()
	{
		States |= EIsoSegmentStates::StatusIsoV;
	}

	bool IsIsoV() const
	{
		return (States & EIsoSegmentStates::StatusIsoV) == EIsoSegmentStates::StatusIsoV;
	}

	void SetAsDegenerated()
	{
		States |= EIsoSegmentStates::Degenerate;
	}

	void ResetDegenerated()
	{
		States &= ~EIsoSegmentStates::Degenerate;
	}

	bool IsDegenerated() const
	{
		return (States & EIsoSegmentStates::Degenerate) == EIsoSegmentStates::Degenerate;
	}

	const FIsoNode& GetFirstNode() const
	{
		return *FirstNode;
	}

	FIsoNode& GetFirstNode()
	{
		return *FirstNode;
	}

	const FIsoNode& GetSecondNode() const
	{
		return *SecondNode;
	}

	FIsoNode& GetSecondNode()
	{
		return *SecondNode;
	}

	void SetFirstNode(FIsoNode& NewNode)
	{
		FirstNode = &NewNode;

		((FLoopNode*)SecondNode)->SetPreviousConnectedNode((FLoopNode*)FirstNode);
		((FLoopNode*)FirstNode)->SetNextConnectedNode((FLoopNode*)SecondNode);
	}

	void SetSecondNode(FIsoNode& NewNode)
	{
		SecondNode = &NewNode;

		((FLoopNode*)SecondNode)->SetPreviousConnectedNode((FLoopNode*)FirstNode);
		((FLoopNode*)FirstNode)->SetNextConnectedNode((FLoopNode*)SecondNode);
	}

	void ReplaceNode(FIsoNode& OldNode, FIsoNode& NewNode)
	{
		if (FirstNode == &OldNode)
		{
			SetFirstNode(NewNode);
		}
		else 
		{
			ensureCADKernel(SecondNode == &OldNode);
			SetSecondNode(NewNode);
		}
	}

	void SwapOrientation()
	{
		FIsoNode* TempNode = SecondNode;
		SecondNode = FirstNode;
		FirstNode = TempNode;

		((FLoopNode*)FirstNode)->SetNextConnectedNode((FLoopNode*)SecondNode);
		((FLoopNode*)SecondNode)->SetPreviousConnectedNode((FLoopNode*)FirstNode);
	}

	const FIsoNode& GetOtherNode(const FIsoNode* Node) const
	{
		return (FirstNode == Node) ? *SecondNode : *FirstNode;
	}

	FIsoNode& GetOtherNode(const FIsoNode* Node)
	{
		return (FirstNode == Node) ? *SecondNode : *FirstNode;
	}

	const ESegmentType GetType() const
	{
		return Type;
	}

	double Get2DLengthSquare(EGridSpace Space, const FGrid& Grid) const
	{
		const FPoint2D& Point1 = FirstNode->Get2DPoint(Space, Grid);
		const FPoint2D& Point2 = SecondNode->Get2DPoint(Space, Grid);
		return Point1.SquareDistance(Point2);
	}

	double Get3DLengthSquare(const FGrid& Grid) const
	{
		const FPoint& Point1 = FirstNode->Get3DPoint(Grid);
		const FPoint& Point2 = SecondNode->Get3DPoint(Grid);
		return Point1.SquareDistance(Point2);
	}
};

inline uint32 GetTypeHash(const FIsoSegment& Segment)
{
	uint32 StartHash = GetTypeHash(Segment.GetFirstNode());
	uint32 EndHash = GetTypeHash(Segment.GetSecondNode());
	if (StartHash < EndHash)
	{
		return HashCombine(StartHash, EndHash);
	}
	else
	{
		return HashCombine(EndHash, StartHash);
	}
};

inline uint32 GetTypeHash(const FIsoSegment& Segment0, const FIsoSegment& Segment1)
{
	uint32 StartHash = GetTypeHash(Segment0);
	uint32 EndHash = GetTypeHash(Segment1);
	if (StartHash < EndHash)
	{
		return HashCombine(StartHash, EndHash);
	}
	else
	{
		return HashCombine(EndHash, StartHash);
	}
};


inline FIsoSegment* FIsoNode::GetSegmentConnectedTo(const FIsoNode* Node) const
{
	for (FIsoSegment* Segment : ConnectedSegments)
	{
		if (&Segment->GetFirstNode() == Node)
		{
			return Segment;
		}
		if (&Segment->GetSecondNode() == Node)
		{
			return Segment;
		}
	}
	return nullptr;
}

} // namespace UE::CADKernel

