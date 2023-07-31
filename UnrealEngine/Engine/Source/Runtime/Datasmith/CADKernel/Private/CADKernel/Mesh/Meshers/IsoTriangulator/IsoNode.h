// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CADKernel/Core/Types.h"
#include "CADKernel/Mesh/MeshEnum.h"
#include "CADKernel/Mesh/Structure/Grid.h"

namespace UE::CADKernel
{
class FGrid;
class FIsoSegment;
class FPoint;
class FPoint2D;

enum class EIsoLink : uint8
{
	IsoUNext = 0,
	IsoVNext = 1,
	IsoUPrevious = 2,
	IsoVPrevious = 3,
};

enum class EIsoNodeStates : uint16
{
	None = 0x0000u,	// No flags.

	LinkedToPreviousU = 0x0001u,
	LinkedToNextU = 0x0002u,
	LinkedToPreviousV = 0x0004u,
	LinkedToNextV = 0x0008u,

	TriangleComplete = 0x000Fu,

	LinkedToLoop = 0x0010u,
	InnerMeshLoop = 0x0020u,

	FirstQuarter = 0x0100u,
	SecondQuarter = 0x0200u,
	ThirdQuarter = 0x0400u,
	FourthQuarter = 0x0800u,

	NearlyIsoU = 0x1000u,  // Loop nodes
	NearlyIsoV = 0x2000u,  // Loop nodes

	Delete = 0x8000u,

	All = 0xFFFFu
};

ENUM_CLASS_FLAGS(EIsoNodeStates);

class FIsoInnerNode;
class FLoopNode;

/**
 * Inner Node of the TopologicalFace, these nodes are IsoNodes as they are build according to the TopologicalFace iso cutting coordinates UV
 */
class FIsoNode
{
protected:
	TArray<FIsoSegment*> ConnectedSegments;
	EIsoNodeStates States;

	int32 Index; // Index of the node either in loop nodes either in inner nodes
	int32 FaceIndex; // Index of the node in the face
	int32 Id;

public:
	FIsoNode(int32 InNodeIndex, int32 InFaceIndex, int32 InNodeId)
		: ConnectedSegments()
		, States(EIsoNodeStates::None)
		, Index(InNodeIndex)
		, FaceIndex(InFaceIndex)
		, Id(InNodeId)
	{
		ConnectedSegments.Reserve(5);
	};

	virtual ~FIsoNode() = default;

	virtual void Delete()
	{
		ConnectedSegments.Empty();
		FaceIndex = -1;

		States = EIsoNodeStates::Delete;
	}

	bool IsDelete() const
	{
		return (States & EIsoNodeStates::Delete) == EIsoNodeStates::Delete;
	}

	const int32 GetIndex() const
	{
		return Index;
	}

	const int32 GetFaceIndex() const
	{
		return FaceIndex;
	}

	const int32 GetId() const
	{
		return Id;
	}

	const TArray<FIsoSegment*>& GetConnectedSegments() const
	{
		return ConnectedSegments;
	}

	FIsoSegment* GetSegmentConnectedTo(const FIsoNode* Node) const;

	void ConnectSegment(FIsoSegment& Segment)
	{
		ConnectedSegments.Add(&Segment);
	}

	void DisconnectSegment(FIsoSegment& Segment)
	{
		ConnectedSegments.RemoveSingle(&Segment);
	}

	const virtual bool IsALoopNode() const = 0;

	void SetAsLinkedToLoop()
	{
		States |= EIsoNodeStates::LinkedToLoop;
	}

	void SetLinkedToLoopInQuarter(int32 Quarter)
	{
		switch (Quarter)
		{
		case 0:
			States |= EIsoNodeStates::FirstQuarter;
		case 1:
			States |= EIsoNodeStates::SecondQuarter;
		case 2:
			States |= EIsoNodeStates::ThirdQuarter;
		default:
			States |= EIsoNodeStates::FourthQuarter;
		}
	}

	bool IsLinkedToLoopInNearlyIso(int32 Iso) const
	{
		if (Iso > 3)
		{
			Iso -= 4;
		}
		switch ((EIsoLink)Iso)
		{
		case EIsoLink::IsoUNext:
			return (States & EIsoNodeStates::LinkedToNextU) == EIsoNodeStates::LinkedToNextU;
		case EIsoLink::IsoVNext:
			return (States & EIsoNodeStates::LinkedToNextV) == EIsoNodeStates::LinkedToNextV;
		case EIsoLink::IsoUPrevious:
			return (States & EIsoNodeStates::LinkedToPreviousU) == EIsoNodeStates::LinkedToPreviousU;
		default:
			return (States & EIsoNodeStates::LinkedToPreviousV) == EIsoNodeStates::LinkedToPreviousV;
		}
	}

	void SetLinkedToIso(EIsoLink Iso) // [3Pi/2 2Pi]
	{
		switch (Iso)
		{
		case EIsoLink::IsoUNext:
			States |= EIsoNodeStates::LinkedToNextU;
			break;
		case EIsoLink::IsoVNext:
			States |= EIsoNodeStates::LinkedToNextV;
			break;
		case EIsoLink::IsoUPrevious:
			States |= EIsoNodeStates::LinkedToPreviousU;
			break;
		default:
			States |= EIsoNodeStates::LinkedToPreviousV;
		}
	}

	void SetLinkedToIso(int32 Iso) // [3Pi/2 2Pi]
	{
		if (Iso > 3)
		{
			Iso -= 4;
		}
		switch ((EIsoLink)Iso)
		{
		case EIsoLink::IsoUNext:
			States |= EIsoNodeStates::LinkedToNextU;
			break;
		case EIsoLink::IsoVNext:
			States |= EIsoNodeStates::LinkedToNextV;
			break;
		case EIsoLink::IsoUPrevious:
			States |= EIsoNodeStates::LinkedToPreviousU;
			break;
		default:
			States |= EIsoNodeStates::LinkedToPreviousV;
		}
	}

	/**
	 * Return the 2d coordinate of the node according to the space
	 */
	virtual const FPoint2D& Get2DPoint(EGridSpace Space, const FGrid& Grid) const = 0;
	virtual void Set2DPoint(EGridSpace Space, FGrid& Grid, const FPoint2D& NewCoordinate) = 0;

	virtual const FPoint& Get3DPoint(const FGrid& Grid) const = 0;
	virtual const FVector3f& GetNormal(const FGrid& Grid) const = 0;

	/**
	 * Only for display purpose as it return a copy of the point
	 */
	virtual const FPoint GetPoint(EGridSpace Space, const FGrid& Grid) const = 0;

	virtual bool operator==(const FIsoNode& OtherNode) const = 0;
	virtual bool IsEqualTo(const FLoopNode& OtherNode) const = 0;
	virtual bool IsEqualTo(const FIsoInnerNode& OtherNode) const = 0;

	virtual uint32 GetTypeHash() const = 0;

};

/**
 * Node of the loop of the TopologicalFace
 */
class FLoopNode : public FIsoNode
{
protected:
	int32 LoopIndex;
	FLoopNode* ConnectedLoopNodes[2];

public:
	FLoopNode(int32 InLoopIndex, int32 InNodeIndex, int32 InFaceIndex, int32 NodeId)
		: FIsoNode(InNodeIndex, InFaceIndex, NodeId)
		, LoopIndex(InLoopIndex)
		, ConnectedLoopNodes{ nullptr, nullptr }
	{
	};

	virtual void Delete() override
	{
		FIsoNode::Delete();

		//LoopIndex = -1;
		ConnectedLoopNodes[0] = nullptr;
		ConnectedLoopNodes[1] = nullptr;
	}

	virtual bool IsEqualTo(const FLoopNode& OtherNode) const
	{
		return ((GetIndex() == OtherNode.GetIndex()) && (GetLoopIndex() == OtherNode.GetLoopIndex()));
	}

	virtual bool IsEqualTo(const FIsoInnerNode& OtherNode) const
	{
		return false;
	}

	bool operator==(const FIsoNode& OtherNode) const
	{
		return OtherNode.IsEqualTo(*this);
	}

	virtual const bool IsALoopNode() const override
	{
		return true;
	}

	const int32 GetLoopIndex() const
	{
		return LoopIndex;
	}

	void SetNextConnectedNode(FLoopNode* NextNode)
	{
		ConnectedLoopNodes[1] = NextNode;
	}
	void SetPreviousConnectedNode(FLoopNode* PreviousNode)
	{
		ConnectedLoopNodes[0] = PreviousNode;
	}

	FLoopNode& GetPreviousNode() const
	{
		return *ConnectedLoopNodes[0];
	}

	FLoopNode& GetNextNode() const
	{
		return *ConnectedLoopNodes[1];
	}

	FIsoSegment& GetPreviousSegment() const
	{
		return *GetSegmentConnectedTo(ConnectedLoopNodes[0]);
	}

	FIsoSegment& GetNextSegment() const
	{
		return *GetSegmentConnectedTo(ConnectedLoopNodes[1]);
	}

	/**
	 * Return the 2d coordinate of the node according to the space
	 */
	virtual const FPoint2D& Get2DPoint(EGridSpace Space, const FGrid& Grid) const override
	{
		return Grid.GetLoop2DPoint(Space, LoopIndex, Index);
	}

	virtual void Set2DPoint(EGridSpace Space, FGrid& Grid, const FPoint2D& NewCoordinate) override
	{
		Grid.SetLoop2DPoint(Space, LoopIndex, Index, NewCoordinate);
	}

	virtual const FPoint& Get3DPoint(const FGrid& Grid) const override
	{
		return Grid.GetLoops3D()[LoopIndex][Index];
	}

	virtual const FVector3f& GetNormal(const FGrid& Grid) const override
	{
		return Grid.GetLoopNormals()[LoopIndex][Index];
	}

	/**
	 * Only for display purpose as it return a copy of the point
	 */
	virtual const FPoint GetPoint(EGridSpace Space, const FGrid& Grid) const override
	{
		switch (Space)
		{
		case EGridSpace::Default2D:
		case EGridSpace::Scaled:
		case EGridSpace::UniformScaled:
			return Grid.GetLoop2DPoint(Space, LoopIndex, Index);
		}
		return FPoint::ZeroPoint;
	}

	/**
	 * Check if the segment starting from this loop node (S) to EndSegmentCoordinate (E) is inside the face
	 * Warning, inner loops (loop index > 0) are not in the same orientation than outer loop
	 *
	 * Inside e.g.:
	 *    X---------------X----------------X        X------------------X-----------X
	 *    |                                |        |                              |
	 *    |         X-----------------X    |        |            X-------------X   |
	 *    |         |                 |    |        |            |             |   |
	 *    X   E-----S                 |    |    or  S---------E  |             |   |
	 *    |         |                 |    |        |            |             |   |
	 *    |         X-----------------X    |        |            X-------------X   |
	 *    |                                |        |                              |
	 *    X---------------X----------------X        X------------------X-----------X
	 *
	 * Outside e.g.:
	 *    X---------------X----------------X        X------------------X-----------X
	 *    |                                |        |                              |
	 *    |         X-----------------X    |        |            X-------------X   |
	 *    |         |                 |    |        |            |             |   |
	 *    |         S------E          |    |    or  |            |             |   S------E
	 *    |         |                 |    |        |            |             |   |
	 *    |         X-----------------X    |        |            X-------------X   |
	 *    |                                |        |                              |
	 *    X---------------X----------------X        X------------------X-----------X
	 */
	bool IsSegmentBeInsideFace(const FPoint2D& EndSegmentCoordinate, const FGrid& Grid, const double FlatAngle) const
	{
		return (!IsPointPInsideSectorABC(GetPreviousNode().Get2DPoint(EGridSpace::UniformScaled, Grid), Get2DPoint(EGridSpace::UniformScaled, Grid), GetNextNode().Get2DPoint(EGridSpace::UniformScaled, Grid), EndSegmentCoordinate, FlatAngle));
	}

	void SetAsIsoU()
	{
		States |= EIsoNodeStates::NearlyIsoU;
	}

	bool IsIsoU() const
	{
		return (States & EIsoNodeStates::NearlyIsoU) == EIsoNodeStates::NearlyIsoU;
	}

	void SetAsIsoV()
	{
		States |= EIsoNodeStates::NearlyIsoV;
	}

	bool IsIsoV() const
	{
		return (States & EIsoNodeStates::NearlyIsoV) == EIsoNodeStates::NearlyIsoV;
	}

	constexpr bool IsIso(EIso Axe) const
	{
		if (Axe == IsoV)
		{
			return IsIsoV();
		}
		else
		{
			return IsIsoU();
		}
	}

	virtual uint32 GetTypeHash() const override
	{
		return HashCombine(GetIndex(), GetLoopIndex() + 1);
	}
};

inline uint32 GetTypeHash(const FIsoNode& Node)
{
	return Node.GetTypeHash();
}

class FIsoInnerNode : public FIsoNode
{
public:
	FIsoInnerNode(int32 NodeIndex, int32 FaceIndex, int32 NodeId)
		: FIsoNode(NodeIndex, FaceIndex, NodeId)
	{
	};

	virtual bool IsEqualTo(const FLoopNode& OtherNode) const
	{
		return false;
	}

	virtual bool IsEqualTo(const FIsoInnerNode& OtherNode) const
	{
		return (GetIndex() == OtherNode.GetIndex());
	}

	bool operator==(const FIsoNode& OtherNode) const
	{
		return OtherNode.IsEqualTo(*this);
	}

	const bool IsIsolated() const
	{
		return (ConnectedSegments.Num() == 0);
	}

	virtual const bool IsALoopNode() const override
	{
		return false;
	}

	bool IsLinkedToPreviousU() const
	{
		return (States & EIsoNodeStates::LinkedToPreviousU) == EIsoNodeStates::LinkedToPreviousU;
	}

	bool IsLinkedToNextU() const
	{
		return (States & EIsoNodeStates::LinkedToNextU) == EIsoNodeStates::LinkedToNextU;
	}

	bool IsLinkedToPreviousV() const
	{
		return (States & EIsoNodeStates::LinkedToPreviousV) == EIsoNodeStates::LinkedToPreviousV;
	}

	bool IsLinkedToNextV() const
	{
		return (States & EIsoNodeStates::LinkedToNextV) == EIsoNodeStates::LinkedToNextV;;
	}

	/** the node is connected in its 4 directions*/
	bool IsComplete() const
	{
		return (States & EIsoNodeStates::TriangleComplete) == EIsoNodeStates::TriangleComplete;
	}

	bool IsLinkedToBoundary() const
	{
		return (States & EIsoNodeStates::LinkedToLoop) == EIsoNodeStates::LinkedToLoop;
	}

	virtual const FPoint2D& Get2DPoint(EGridSpace Space, const FGrid& Grid) const override
	{
		return Grid.GetInner2DPoint(Space, Index);
	}

	virtual void Set2DPoint(EGridSpace Space, FGrid& Grid, const FPoint2D& NewCoordinate) override
	{
		return Grid.SetInner2DPoint(Space, Index, NewCoordinate);
	}

	virtual const FPoint& Get3DPoint(const FGrid& Grid) const override
	{
		return Grid.GetInner3DPoint(Index);
	}

	virtual const FVector3f& GetNormal(const FGrid& Grid) const override
	{
		return Grid.GetPointNormal(Index);
	}

	/**
	 * Only for display purpose as it return a copy of the point
	 */
	virtual const FPoint GetPoint(EGridSpace Space, const FGrid& Grid) const override
	{
		switch (Space)
		{
		case EGridSpace::Default2D:
		case EGridSpace::Scaled:
		case EGridSpace::UniformScaled:
			return Grid.GetInner2DPoint(Space, Index);
		}
		return FPoint::ZeroPoint;
	}

	void OffsetId(int32 StartId)
	{
		Id += StartId;
	}

	virtual uint32 GetTypeHash() const override
	{
		return ::GetTypeHash(GetIndex());
	}
};


} // namespace UE::CADKernel

