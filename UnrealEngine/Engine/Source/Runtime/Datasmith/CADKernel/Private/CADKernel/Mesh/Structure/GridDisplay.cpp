// Copyright Epic Games, Inc. All Rights Reserved.
#ifdef CADKERNEL_DEV

#include "CADKernel/Mesh/Structure/Grid.h"

#include "CADKernel/Mesh/Meshers/IsoTriangulator/IsoNode.h"
#include "CADKernel/Mesh/Meshers/IsoTriangulator/IsoSegment.h"
#include "CADKernel/UI/Display.h"

namespace UE::CADKernel
{

void FGrid::DisplayIsoNode(EGridSpace DisplaySpace, const int32 PointIndex, FIdent Ident, EVisuProperty Property) const
{
	if (!bDisplay)
	{
		return;
	}
	DisplayPoint(Points2D[DisplaySpace][PointIndex] * DisplayScale, Ident);
}

void FGrid::DisplayIsoNode(EGridSpace Space, const FIsoNode& Node, FIdent Ident, EVisuProperty Property) const
{
	if (!bDisplay)
	{
		return;
	}
	DisplayPoint(Node.GetPoint(Space, *this) * DisplayScale, Property, Ident);
}

void FGrid::DisplayIsoNodes(EGridSpace Space, const TArray<const FIsoNode*>& Nodes, EVisuProperty Property) const
{
	if (!bDisplay)
	{
		return;
	}

	for (const FIsoNode* Node : Nodes)
	{
		DisplayIsoNode(Space, *Node, Node->GetId(), Property);
	}
}

void FGrid::DisplayIsoSegment(EGridSpace Space, const FIsoSegment& Segment, FIdent Ident, EVisuProperty Property, bool bDisplayOrientation) const
{
	if (!bDisplay)
	{
		return;
	}
	DisplaySegment(Segment.GetFirstNode().GetPoint(Space, *this) * DisplayScale, Segment.GetSecondNode().GetPoint(Space, *this) * DisplayScale, Ident, Property, bDisplayOrientation);
}

void FGrid::DisplayIsoSegment(EGridSpace Space, const FIsoNode& NodeA, const FIsoNode& NodeB, FIdent Ident, EVisuProperty Property) const
{
	if (!bDisplay)
	{
		return;
	}
	DisplaySegment(NodeA.GetPoint(Space, *this) * DisplayScale, NodeB.GetPoint(Space, *this) * DisplayScale, Ident, Property);
}

void FGrid::DisplayTriangle(EGridSpace Space, const FIsoNode& NodeA, const FIsoNode& NodeB, const FIsoNode& NodeC) const
{
	TArray<FPoint> Points;
	Points.SetNum(3);
	Points[0] = NodeA.GetPoint(Space, *this) * DisplayScale;
	Points[1] = NodeB.GetPoint(Space, *this) * DisplayScale;
	Points[2] = NodeC.GetPoint(Space, *this) * DisplayScale;
	DrawElement(2, Points, EVisuProperty::Element);

	DisplaySegment(Points[0], Points[1], 0, EVisuProperty::EdgeMesh);
	DisplaySegment(Points[1], Points[2], 0, EVisuProperty::EdgeMesh);
	DisplaySegment(Points[2], Points[0], 0, EVisuProperty::EdgeMesh);
}


void FGrid::DisplayIsoSegments(EGridSpace Space, const TArray<FIsoSegment*>& InSegments, bool bDisplayNode, bool bDisplayOrientation, EVisuProperty Property) const
{
	if (!bDisplay)
	{
		return;
	}
	int32 Index = 0;
	for (FIsoSegment* Segment : InSegments)
	{
		if (Segment)
		{
			DisplaySegment(Segment->GetFirstNode().GetPoint(Space, *this) * DisplayScale, Segment->GetSecondNode().GetPoint(Space, *this) * DisplayScale, Index++, Property, bDisplayOrientation);
			if(bDisplayNode)
			{
				DisplayPoint(Segment->GetFirstNode().GetPoint(Space, *this) * DisplayScale, (EVisuProperty)(Property - 1), Segment->GetFirstNode().GetIndex());
				DisplayPoint(Segment->GetSecondNode().GetPoint(Space, *this) * DisplayScale, (EVisuProperty)(Property - 1), Segment->GetSecondNode().GetIndex());
			}
		}
	}
}

//void FGrid::DisplayGridSegments(const TCHAR* Message, EGridSpace Space, const TArray<FIsoSegment*>& Segments, bool bDisplayNode, bool bDisplayOrientation, EVisuProperty Property) const
//{
//	if (bDisplay)
//	{
//		F3DDebugSession _(Message);
//		DisplayGridSegments(Space, Segments, bDisplayNode, bDisplayOrientation, Property);
//	}
//}

void FGrid::DisplayGridPolyline(EGridSpace Space, const FLoopNode& StartNode, bool bDisplayNode, EVisuProperty Property) const
{
	if (!bDisplay)
	{
		return;
	}
	int32 Index = 1;
	const FLoopNode* Node = &StartNode.GetNextNode();
	const FLoopNode* NextNode = nullptr;
	DisplayIsoSegment(Space, StartNode, *Node, Index++, Property);
	for (; Node != &StartNode; Node = NextNode)
	{
		NextNode = &Node->GetNextNode();
		DisplayIsoSegment(Space, *Node, *NextNode, Index++, Property);
	}

	DisplayIsoNode(Space, StartNode, StartNode.GetIndex(), Property);
	if(bDisplayNode)
	{
		for (; Node != &StartNode; Node = &Node->GetNextNode())
		{
			DisplayIsoNode(Space, *Node, Node->GetIndex(), (EVisuProperty)(Property - 1));
		}
	}
}

void FGrid::DisplayGridPolyline(EGridSpace Space, const TArray<FLoopNode*>& Nodes, bool bDisplayNode, EVisuProperty Property) const
{
	if (!bDisplay)
	{
		return;
	}

	{
		//F3DDebugSession Q(TEXT("GetNextNode"));
		for (const FLoopNode* Node : Nodes)
		{
			if (!Node->IsDelete())
			{
				DisplayIsoSegment(Space, *Node, Node->GetNextNode(), 0, Property);
			}
		}
	}

	//{
	//	F3DDebugSession Q(TEXT("GetPreviousNode"));
	//	for (const FLoopNode* Node : Nodes)
	//	{
	//		if (!Node->IsDelete())
	//		{
	//			DisplayIsoSegment(Space, *Node, Node->GetPreviousNode(), 0, Property);
	//		}
	//	}
	//}

	{
		//F3DDebugSession Q(TEXT("Node"));
		if (bDisplayNode)
		{
			int32 Index = 0;
			for (const FLoopNode* Node : Nodes)
			{
				//DisplayIsoNode(Space, *Node, Node->GetIndex(), Node->IsDelete() ? EVisuProperty::PurplePoint : (EVisuProperty)((int32)Property - 1));
				DisplayIsoNode(Space, *Node, Index++, Node->IsDelete() ? EVisuProperty::PurplePoint : (EVisuProperty)((int32)Property - 1));
			}
		}
	}
}

void FGrid::DisplayGridPolyline(EGridSpace Space, const TArray<FLoopNode>& Nodes, bool bDisplayNode, EVisuProperty Property) const
{
	if (!bDisplay)
	{
		return;
	}

	{
		//F3DDebugSession Q(TEXT("GetNextNode"));
		for (const FLoopNode& Node : Nodes)
		{
			if (!Node.IsDelete())
			{
				DisplayIsoSegment(Space, Node, Node.GetNextNode(), 0, Property);
			}
		}
	}

	{
		//F3DDebugSession Q(TEXT("Node"));
		if (bDisplayNode)
		{
			for (const FLoopNode& Node : Nodes)
			{
				DisplayIsoNode(Space, Node, Node.GetIndex(), Node.IsDelete() ? EVisuProperty::PurplePoint : (EVisuProperty)((int32)Property - 1));
			}
		}
	}
}

void FGrid::DisplayGridPolyline(EGridSpace Space, const TArray<FIsoNode*>& Nodes, bool bDisplayNode, EVisuProperty Property) const
{
	if (!bDisplay)
	{
		return;
	}
	int32 Index = 1;
	for (; Index < Nodes.Num(); ++Index)
	{
		DisplayIsoSegment(Space, *Nodes[Index - 1], *Nodes[Index], Index, Property);
	}
	DisplayIsoSegment(Space, *Nodes[Index - 1], *Nodes[0], Index, Property);

	if (bDisplayNode)
	{
		for (const FIsoNode* Node : Nodes)
		{
			DisplayIsoNode(Space, *Node, Node->GetIndex(), (EVisuProperty)(Property - 1));
		}
	}
}

#ifdef TOTOT
void FGrid::DisplayGridLoopNodesAndSegments(const TCHAR* Message, bool bOneNode, bool bSplitBySegment, bool bDisplayNext, bool bDisplayPrevious) const
{
	if (bDisplay)
	{
		F3DDebugSession _(Message);
		{
			{
				F3DDebugSession A(!bOneNode, TEXT("Node Cloud"));
				int32 Index = 0;
				for (const FLoopNode& Node : LoopNodes)
				{
					if (Node.IsDelete())
					{
						Display(EGridSpace::UniformScaled, Node, Index++, EVisuProperty::PurplePoint);
					}
					Display(EGridSpace::UniformScaled, Node, Index++, EVisuProperty::BluePoint);
				}
			}

			{
				F3DDebugSession C(!bOneNode, TEXT("Segment"));
				int32 Index = 0;
				for (FIsoSegment* Segment : LoopSegments)
				{
					F3DDebugSession D(bSplitBySegment, TEXT("Segment"));
					Display(EGridSpace::UniformScaled, Segment->GetFirstNode(), Segment->GetSecondNode(), Index++, EVisuProperty::BlueCurve);
				}
			}

			if (bDisplayNext)
			{
				F3DDebugSession A(TEXT("Node Next"));
				int32 Index = 0;
				for (FIsoSegment* Segment : LoopSegments)
				{
					F3DDebugSession B(bSplitBySegment, TEXT("Segment"));
					Display(EGridSpace::UniformScaled, Segment->GetFirstNode(), ((const FLoopNode&)Segment->GetFirstNode()).GetNextNode(), Index++, EVisuProperty::BlueCurve);
				}
			}

			if (bDisplayPrevious)
			{
				F3DDebugSession C(TEXT("Node Forward"));
				int32 Index = 0;
				for (FIsoSegment* Segment : LoopSegments)
				{
					F3DDebugSession B(bSplitBySegment, TEXT("Segment"));
					Display(EGridSpace::UniformScaled, Segment->GetSecondNode(), ((const FLoopNode&)Segment->GetSecondNode()).GetPreviousNode(), Index++, EVisuProperty::BlueCurve);
				}
			}

		}
		Wait(false);
	}
}
#endif

void FGrid::DisplayNodes(const TCHAR* Message, EGridSpace Space, const TArray<const FIsoNode*>& Nodes, EVisuProperty Property) const
{
	if (!bDisplay)
	{
		return;
	}

	Open3DDebugSession(Message);
	for (int32 Index = 0; Index < Nodes.Num(); ++Index)
	{
		DisplayIsoNode(Space, *Nodes[Index], Nodes[Index]->GetIndex(), Property);
	}
	Close3DDebugSession();
}

void FGrid::DisplayGridPoints(EGridSpace DisplaySpace) const
{
	if (!bDisplay)
	{
		return;
	}
	int32 NbNum = 0;
	{
		F3DDebugSession _(TEXT("FGrid::FindInnerDomainPoints Inside Point"));
		for (int32 Index = 0; Index < CuttingSize; ++Index)
		{
			if (IsInsideFace[Index])
			{
				DisplayPoint(Points2D[DisplaySpace][Index] * DisplayScale, EVisuProperty::BluePoint, Index);
				NbNum++;
			}
		}
	}
	ensureCADKernel(NbNum == CountOfInnerNodes);

	{
		F3DDebugSession _(TEXT("FGrid::FindInnerDomainPoints Outside Point"));
		for (int32 Index = 0; Index < CuttingSize; ++Index)
		{
			if (!IsInsideFace[Index])
			{
				DisplayPoint(Points2D[DisplaySpace][Index] * DisplayScale, EVisuProperty::OrangePoint, Index);
			}
		}
	}
}

void FGrid::DisplayGridNormal() const
{
	if (!bDisplay)
	{
		return;
	}

	double NormalLength = FSystem::Get().GetVisu()->GetParameters()->NormalLength;
	{
		F3DDebugSession _(TEXT("FGrid::Inner Normal"));

		for (int32 Index = 0; Index < CuttingSize; ++Index)
		{
			F3DDebugSegment GraphicSegment(Index);
			FVector3f Normal = Normals[Index];
			Normal.Normalize();
			Normal *= NormalLength;
			DrawSegment(Points3D[Index], Points3D[Index] + Normal, EVisuProperty::GreenCurve);
		}
	}

	{
		F3DDebugSession _(TEXT("FGrid::Loop Normal"));
		for (int32 LoopIndex = 0; LoopIndex < FaceLoops3D.Num(); ++LoopIndex)
		{
			const TArray<FPoint>& LoopPoints = FaceLoops3D[LoopIndex];
			const TArray<FVector3f>& LoopNormals = NormalsOfFaceLoops[LoopIndex];

			for (int32 Index = 0; Index < LoopPoints.Num(); ++Index)
			{
				F3DDebugSegment GraphicSegment(Index);
				FVector3f Normal = LoopNormals[Index];
				Normal.Normalize();
				Normal *= NormalLength;
				DrawSegment(LoopPoints[Index], LoopPoints[Index] + Normal, EVisuProperty::YellowCurve);
			}
		}
	}
}

void FGrid::DisplayInnerPoints(TCHAR* Message, EGridSpace DisplaySpace) const
{
	if (!bDisplay)
	{
		return;
	}
	DisplayInnerDomainPoints(Message, Points2D[DisplaySpace]);
}

}

#endif