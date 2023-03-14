// Copyright Epic Games, Inc. All Rights Reserved.

#include "CADKernel/Mesh/Meshers/IsoTriangulator.h"

#include "CADKernel/Core/Chrono.h"
#include "CADKernel/Math/MathConst.h"
#include "CADKernel/Math/Point.h"
#include "CADKernel/Mesh/Meshers/BowyerWatsonTriangulator.h"
#include "CADKernel/Mesh/Meshers/IsoTriangulator/IsoCell.h"
#include "CADKernel/Mesh/Meshers/IsoTriangulator/IsoNode.h"
#include "CADKernel/Mesh/Meshers/IsoTriangulator/IsoSegment.h"
#include "CADKernel/Mesh/Structure/Grid.h"
#include "CADKernel/Mesh/Structure/EdgeMesh.h"
#include "CADKernel/Mesh/Structure/FaceMesh.h"
#include "CADKernel/Mesh/Structure/LoopCleaner.h"
#include "CADKernel/Topo/TopologicalEdge.h"
#include "CADKernel/Topo/TopologicalFace.h"
#include "CADKernel/Topo/TopologicalFace.h"
#include "CADKernel/UI/Display.h"
#include "CADKernel/Utils/ArrayUtils.h"

namespace UE::CADKernel
{

const double FIsoTriangulator::GeometricToMeshingToleranceFactor = 10.;

#ifdef DEBUG_BOWYERWATSON
bool FBowyerWatsonTriangulator::bDisplay = false;
#endif

FIsoTriangulator::FIsoTriangulator(FGrid& InGrid, TSharedRef<FFaceMesh> EntityMesh)
	: Grid(InGrid)
	, Mesh(EntityMesh)
	, LoopSegmentsIntersectionTool(InGrid)
	, InnerSegmentsIntersectionTool(InGrid)
	, InnerToLoopSegmentsIntersectionTool(InGrid)
	, InnerToOuterSegmentsIntersectionTool(InGrid)
	, GeometricTolerance(InGrid.GetFace().GetCarrierSurface()->Get3DTolerance())
	, SquareGeometricTolerance(FMath::Square(GeometricTolerance))
	, SquareGeometricTolerance2(2. * SquareGeometricTolerance)
	, MeshingTolerance(GeometricTolerance* GeometricToMeshingToleranceFactor)
	, SquareMeshingTolerance(FMath::Square(MeshingTolerance))
{
	FinalInnerSegments.Reserve(3 * Grid.InnerNodesCount());
	IndexOfLowerLeftInnerNodeSurroundingALoop.Reserve(Grid.GetLoopCount());

#ifdef DEBUG_ISOTRIANGULATOR

#ifdef DEBUG_ONLY_SURFACE_TO_DEBUG
	if (Grid.GetFace().GetId() == FaceToDebug)
#endif
	{
		bDisplay = true;
	}

#endif
}

bool FIsoTriangulator::Triangulate()
{
	EGridSpace DisplaySpace = EGridSpace::UniformScaled;
#ifdef DEBUG_ISOTRIANGULATOR
	F3DDebugSession _(bDisplay, FString::Printf(TEXT("Triangulate %d"), Grid.GetFace().GetId()));
#endif

	FTimePoint StartTime = FChrono::Now();

	// =============================================================================================================
	// Build the first elements (IsoNodes (i.e. Inner nodes), Loops nodes, and knows segments 
	// =============================================================================================================

	BuildNodes();

#ifdef DEBUG_ISOTRIANGULATOR
	DisplayIsoNodes(DisplaySpace);
#endif

	FillMeshNodes();
	BuildLoopSegments();

#ifdef DEBUG_ISOTRIANGULATOR
	DisplayLoops(TEXT("FIsoTrianguler::LoopSegments"), true, false);
#endif

	FLoopCleaner LoopCleaner(*this);
	if (!LoopCleaner.Run())
	{
#ifdef CADKERNEL_DEV
		MesherReport->Logs.AddDegeneratedLoop();
#endif
		FMessage::Printf(EVerboseLevel::Log, TEXT("The meshing of the surface %d failed due to a degenerated loop\n"), Grid.GetFace().GetId());
		return false;
	}

#ifdef DEBUG_ISOTRIANGULATOR
	DisplayLoops(TEXT("FIsoTrianguler::LoopSegments after fix intersection"), true, false);
	Grid.DisplayIsoSegments(TEXT("FIsoTrianguler::LoopSegments orientation"), DisplaySpace, LoopSegments, false, true, EVisuProperty::YellowCurve);
	Wait(false);
#endif

	BuildThinZoneSegments();

	//Fill Intersection tool
	LoopSegmentsIntersectionTool.Empty(LoopSegments.Num());
	LoopSegmentsIntersectionTool.AddSegments(LoopSegments);
	LoopSegmentsIntersectionTool.AddSegments(ThinZoneSegments);
	LoopSegmentsIntersectionTool.Sort();

#ifdef DEBUG_ISOTRIANGULATOR
	Grid.DisplayIsoSegments(TEXT("FIsoTrianguler::ThinZoneSegments"), DisplaySpace, ThinZoneSegments, false);
	LoopSegmentsIntersectionTool.Display(TEXT("FIsoTrianguler::IntersectionTool Loop"));
#endif

	BuildInnerSegments();

#ifdef DEBUG_ISOTRIANGULATOR
	{
		F3DDebugSession A(bDisplay, TEXT("FIsoTrianguler::FinalInnerSegments"));
		Grid.DisplayIsoSegments(DisplaySpace, FinalInnerSegments, true, false, EVisuProperty::BlueCurve);
	}
	InnerToOuterSegmentsIntersectionTool.Display(TEXT("FIsoTrianguler::IntersectionTool InnerToOutter"));
	Chronos.TriangulateDuration1 = FChrono::Elapse(StartTime);
#endif

	// =============================================================================================================
	// =============================================================================================================

	BuildInnerSegmentsIntersectionTool();
#ifdef DEBUG_ISOTRIANGULATOR
	InnerSegmentsIntersectionTool.Display(TEXT("FIsoTrianguler::IntersectionTool Inner"));
#endif

#ifdef DEBUG_ISOTRIANGULATOR
	Chronos.TriangulateDuration2 = FChrono::Elapse(StartTime);
#endif

	// =============================================================================================================
	// 	   For each cell
	// 	      - Connect loops together and to cell vertices
	// 	           - Find subset of node of each loop
	// 	           - build Delaunay connection
	// 	           - find the shortest segment to connect each connected loop by Delaunay
	// =============================================================================================================
#ifdef DEBUG_ISOTRIANGULATOR
	Grid.DisplayIsoSegments(TEXT("FIsoTrianguler::Final To Loops Before"), DisplaySpace, FinalToLoops, false, false, EVisuProperty::YellowCurve);
	//Wait(bDisplay);
#endif

	ConnectCellLoops();

#ifdef DEBUG_ISOTRIANGULATOR
	Grid.DisplayIsoSegments(TEXT("FIsoTrianguler::Final Iso ToLink Inner To Loops"), DisplaySpace, FinalToLoops, false, false, EVisuProperty::YellowCurve);
#endif

	// =============================================================================================================
	// Make the final tessellation 
	// =============================================================================================================

	// Triangulate between inner grid boundary and loops
	TriangulateOverCycle(EGridSpace::Scaled);

	// Finalize the mesh by the tessellation of the inner grid
	TriangulateInnerNodes();

#ifdef DEBUG_ISOTRIANGULATOR
	if (bDisplay)
	{
		F3DDebugSession A(TEXT("Mesh 3D"));
		DisplayMesh(*Mesh);
	}
	//Wait(bDisplay);
#endif

#ifdef CADKERNEL_DEV
	//Chronos.PrintTimeElapse();
#endif

	return true;
}

void FIsoTriangulator::BuildNodes()
{
	FTimePoint StartTime = FChrono::Now();

	LoopNodeCount = 0;
	for (const TArray<FPoint2D>& Loop : Grid.GetLoops2D(EGridSpace::Default2D))
	{
		LoopNodeCount += (int32)Loop.Num();
	}
	LoopStartIndex.Reserve(Grid.GetLoops2D(EGridSpace::Default2D).Num());
	LoopNodes.Reserve((int32)(LoopNodeCount * 1.2 + 5)); // reserve more in case it need to create complementary nodes

	// Loop nodes
	int32 FaceIndex = 0;
	int32 LoopIndex = 0;
	for (const TArray<FPoint2D>& Loop : Grid.GetLoops2D(EGridSpace::Default2D))
	{
		LoopStartIndex.Add(LoopNodeCount);
		const TArray<int32>& LoopIds = Grid.GetNodeIdsOfFaceLoops()[LoopIndex];
		FLoopNode* NextNode = nullptr;
		FLoopNode* FirstNode = &LoopNodes.Emplace_GetRef(LoopIndex, 0, FaceIndex++, LoopIds[0]);
		FLoopNode* PreviousNode = FirstNode;
		for (int32 Index = 1; Index < Loop.Num(); ++Index)
		{
			NextNode = &LoopNodes.Emplace_GetRef(LoopIndex, Index, FaceIndex++, LoopIds[Index]);
			PreviousNode->SetNextConnectedNode(NextNode);
			NextNode->SetPreviousConnectedNode(PreviousNode);
			PreviousNode = NextNode;
		}
		PreviousNode->SetNextConnectedNode(FirstNode);
		FirstNode->SetPreviousConnectedNode(PreviousNode);
		LoopIndex++;
	}

	// Inner node
	InnerNodes.Reserve(Grid.InnerNodesCount());
	GlobalIndexToIsoInnerNodes.Init(nullptr, Grid.GetTotalCuttingCount());

	InnerNodeCount = 0;
	for (int32 Index = 0; Index < (int32)Grid.GetTotalCuttingCount(); ++Index)
	{
		if (Grid.IsNodeInsideFace(Index))
		{
			FIsoInnerNode& Node = InnerNodes.Emplace_GetRef(Index, FaceIndex++, InnerNodeCount++);
			GlobalIndexToIsoInnerNodes[Index] = &Node;
		}
	}

#ifdef CADKERNEL_DEV
	Chronos.BuildIsoNodesDuration += FChrono::Elapse(StartTime);
#endif
}

void FIsoTriangulator::FillMeshNodes()
{
	int32 TriangleNum = 50 + (int32)((2 * InnerNodeCount + LoopNodeCount) * 1.1);
	Mesh->Init(TriangleNum, InnerNodeCount + LoopNodeCount);

	TArray<FPoint>& InnerNodeCoordinates = Mesh->GetNodeCoordinates();
	InnerNodeCoordinates.Reserve(InnerNodeCount);
	for (int32 Index = 0; Index < (int32)Grid.GetInner3DPoints().Num(); ++Index)
	{
		if (Grid.IsNodeInsideFace(Index))
		{
			InnerNodeCoordinates.Emplace(Grid.GetInner3DPoints()[Index]);
		}
	}

	int32 StartId = Mesh->RegisterCoordinates();
	for (FIsoInnerNode& Node : InnerNodes)
	{
		Node.OffsetId(StartId);
	}

	Mesh->VerticesGlobalIndex.SetNum(InnerNodeCount + LoopNodeCount);
	int32 Index = 0;
	for (FLoopNode& Node : LoopNodes)
	{
		Mesh->VerticesGlobalIndex[Index++] = Node.GetId();
	}

	for (FIsoInnerNode& Node : InnerNodes)
	{
		Mesh->VerticesGlobalIndex[Index++] = Node.GetId();
	}

	for (FLoopNode& Node : LoopNodes)
	{
		Mesh->Normals.Emplace(Node.GetNormal(Grid));
	}

	for (FIsoInnerNode& Node : InnerNodes)
	{
		Mesh->Normals.Emplace(Node.GetNormal(Grid));
	}

	for (FLoopNode& Node : LoopNodes)
	{
		const FPoint2D& UVCoordinate = Node.Get2DPoint(EGridSpace::Scaled, Grid);
		Mesh->UVMap.Emplace(UVCoordinate.U, UVCoordinate.V);
	}

	for (FIsoInnerNode& Node : InnerNodes)
	{
		const FPoint2D& UVCoordinate = Node.Get2DPoint(EGridSpace::Scaled, Grid);
		Mesh->UVMap.Emplace(UVCoordinate.U, UVCoordinate.V);
	}
}

namespace IsoTriangulatorImpl
{
const double MaxSlopeToBeIso = 0.125;

const double LimitValueMin(double Slope)
{
	return Slope - MaxSlopeToBeIso;
}

const double LimitValueMax(double Slope)
{
	return Slope + MaxSlopeToBeIso;
}
}

void FIsoTriangulator::BuildLoopSegments()
{
	FTimePoint StartTime = FChrono::Now();

	LoopSegments.Reserve(LoopNodeCount);

	int32 LoopIndex = 0;
	for (FLoopNode& Node : LoopNodes)
	{
		if (Node.IsDelete())
		{
			continue;
		}

		FIsoSegment& Segment = IsoSegmentFactory.New();
		Segment.Init(Node, Node.GetNextNode(), ESegmentType::Loop);
		Segment.ConnectToNode();
		LoopSegments.Add(&Segment);
	}

	for (FIsoSegment* Segment : LoopSegments)
	{
		double SegmentSlop = ComputeSlope(Segment->GetFirstNode().Get2DPoint(EGridSpace::UniformScaled, Grid), Segment->GetSecondNode().Get2DPoint(EGridSpace::UniformScaled, Grid));
		if (SegmentSlop < IsoTriangulatorImpl::MaxSlopeToBeIso)
		{
			Segment->SetAsIsoU();
		}
		if (SegmentSlop < IsoTriangulatorImpl::LimitValueMax(2.) && SegmentSlop > IsoTriangulatorImpl::LimitValueMin(2.))
		{
			Segment->SetAsIsoV();
		}
		if (SegmentSlop < IsoTriangulatorImpl::LimitValueMax(4.) && SegmentSlop > IsoTriangulatorImpl::LimitValueMin(4.))
		{
			Segment->SetAsIsoU();
		}
		if (SegmentSlop < IsoTriangulatorImpl::LimitValueMax(6.) && SegmentSlop > IsoTriangulatorImpl::LimitValueMin(6.))
		{
			Segment->SetAsIsoV();
		}
		if (SegmentSlop > IsoTriangulatorImpl::LimitValueMin(8.))
		{
			Segment->SetAsIsoU();
		}
	}

	for (FLoopNode& Node : LoopNodes)
	{
		if (Node.IsDelete())
		{
			continue;
		}

		if (Node.GetConnectedSegments()[0]->IsIsoU() && Node.GetConnectedSegments()[1]->IsIsoU())
		{
			Node.SetAsIsoU();
		}
		else if (Node.GetConnectedSegments()[0]->IsIsoV() && Node.GetConnectedSegments()[1]->IsIsoV())
		{
			Node.SetAsIsoV();
		}
	}

#ifdef CADKERNEL_DEV
	Chronos.BuildLoopSegmentsDuration += FChrono::Elapse(StartTime);
#endif
}

void FIsoTriangulator::BuildThinZoneSegments()
{
	ThinZoneSegments.Reserve((int32)(0.6 * (double)LoopNodeCount));

	TMap<int32, FLoopNode*> IndexToNode;
	for (FLoopNode& Node : LoopNodes)
	{
		if (Node.IsDelete())
		{
			continue;
		}

		IndexToNode.Add(Node.GetFaceIndex(), &Node);
	}

	TFunction<void(FIsoNode*, FIsoNode*)> AddSegment = [this](FIsoNode* NodeA, FIsoNode* NodeB)
	{
		if (!NodeA)
		{
			return;
		}

		if (!NodeB)
		{
			return;
		}

		if (NodeA->GetSegmentConnectedTo(NodeB))
		{
			return;
		}

		FIsoSegment& Segment = IsoSegmentFactory.New();
		Segment.Init(*NodeA, *NodeB, ESegmentType::ThinZone);
		Segment.ConnectToNode();
		ThinZoneSegments.Add(&Segment);
	};

	for (const TSharedPtr<FTopologicalLoop>& Loop : Grid.GetFace().GetLoops())
	{
		for (const FOrientedEdge& OrientedEdge : Loop->GetEdges())
		{
			const TSharedPtr<FTopologicalEdge>& Edge = OrientedEdge.Entity;
			if (!Edge->IsThinZone())
			{
				continue;
			}

			const TArray<FCuttingPoint>& CuttingPoints = Edge->GetLinkActiveEdge()->GetCuttingPoints();
			const TArray<int32>& NodeIds = Edge->GetMesh()->EdgeVerticesIndex;
			for (int32 Index = 0; Index < NodeIds.Num(); ++Index)
			{
				if (CuttingPoints[Index].OppositNodeIndex > 0)
				{
					AddSegment(IndexToNode[NodeIds[Index]], IndexToNode[CuttingPoints[Index].OppositNodeIndex]);
				}
				if (CuttingPoints[Index].OppositNodeIndex2 > 0)
				{
					AddSegment(IndexToNode[NodeIds[Index]], IndexToNode[CuttingPoints[Index].OppositNodeIndex2]);
				}
			}
		}
	}

	LoopSegmentsIntersectionTool.AddSegments(ThinZoneSegments);
	LoopSegmentsIntersectionTool.Sort();
}

void FIsoTriangulator::BuildInnerSegments()
{

#ifdef DEBUG_BUILDINNERSEGMENTS
	if (bDisplay)
	{
		Grid.DisplayInnerPoints(TEXT("BuildInnerSegments::Points"), EGridSpace::UniformScaled);
	}

	F3DDebugSession _(bDisplay, TEXT("BuildInnerSegments"));
#endif

	FTimePoint StartTime = FChrono::Now();

	// Build segments according to the Grid following u then following v
	// Build segment must not be in intersection with the loop
	int32 NumU = Grid.GetCuttingCount(EIso::IsoU);
	int32 NumV = Grid.GetCuttingCount(EIso::IsoV);

	LoopSegmentsIntersectionTool.Reserve(InnerSegmentsIntersectionTool.Count());

	TFunction<bool(const FPoint2D&, const FPoint2D&, const ESegmentType)> AlmostHitsLoop = [&](const FPoint2D& Node1, const FPoint2D& Node2, const ESegmentType InType) -> bool
	{
		if (InType == ESegmentType::IsoU)
		{
			for (const TArray<FPoint2D>& Loop : Grid.GetLoops2D(EGridSpace::UniformScaled))
			{
				for (const FPoint2D& LoopPoint : Loop)
				{
					if (FMath::IsNearlyEqual(LoopPoint.V, Node1.V, GeometricTolerance))
					{
						if (Node1.U - DOUBLE_SMALL_NUMBER < LoopPoint.U && LoopPoint.U < Node2.U + DOUBLE_SMALL_NUMBER)
						{
							return true;
						}
					}
				}
			}
		}
		else
		{
			for (const TArray<FPoint2D>& Loop : Grid.GetLoops2D(EGridSpace::UniformScaled))
			{
				for (const FPoint2D& LoopPoint : Loop)
				{
					if (FMath::IsNearlyEqual(LoopPoint.U, Node1.U, GeometricTolerance))
					{
						if (Node1.V - DOUBLE_SMALL_NUMBER < LoopPoint.V && LoopPoint.V < Node2.V + DOUBLE_SMALL_NUMBER)
						{
							return true;
						}
					}
				}
			}
		}
		return false;
	};

	TFunction<void(const int32, const int32, const ESegmentType)> BuildSegmentIfValid = [&](const int32 IndexNode1, const int32 IndexNode2, const ESegmentType InType)
	{
		if (!Grid.IsNodeInsideFace(IndexNode1) || !Grid.IsNodeInsideFace(IndexNode2))
		{
#ifdef DEBUG_BUILDINNERSEGMENTS
			if (bDisplay)
			{
				DisplaySegment(Grid.GetInner2DPoint(EGridSpace::UniformScaled, IndexNode1) * Grid.DisplayScale, Grid.GetInner2DPoint(EGridSpace::UniformScaled, IndexNode2) * Grid.DisplayScale, IndexNode1, EVisuProperty::BlueCurve);
			}
#endif
			InnerToOuterSegmentsIntersectionTool.AddSegment(Grid.GetInner2DPoint(EGridSpace::UniformScaled, IndexNode1), Grid.GetInner2DPoint(EGridSpace::UniformScaled, IndexNode2));
			return;
		}

		if (Grid.IsNodeCloseToLoop(IndexNode1) && Grid.IsNodeCloseToLoop(IndexNode2))
		{

			if (LoopSegmentsIntersectionTool.DoesIntersect(Grid.GetInner2DPoint(EGridSpace::UniformScaled, IndexNode1), Grid.GetInner2DPoint(EGridSpace::UniformScaled, IndexNode2))
				|| AlmostHitsLoop(Grid.GetInner2DPoint(EGridSpace::UniformScaled, IndexNode1), Grid.GetInner2DPoint(EGridSpace::UniformScaled, IndexNode2), InType))
			{
				InnerToOuterSegmentsIntersectionTool.AddSegment(Grid.GetInner2DPoint(EGridSpace::UniformScaled, IndexNode1), Grid.GetInner2DPoint(EGridSpace::UniformScaled, IndexNode2));
#ifdef DEBUG_BUILDINNERSEGMENTS
				if (bDisplay)
				{
					DisplaySegment(Grid.GetInner2DPoint(EGridSpace::UniformScaled, IndexNode1) * Grid.DisplayScale, Grid.GetInner2DPoint(EGridSpace::UniformScaled, IndexNode2) * Grid.DisplayScale, IndexNode1, EVisuProperty::YellowCurve);
				}
#endif
				return;
			}
			else
			{
#ifdef DEBUG_BUILDINNERSEGMENTS
				if (bDisplay)
				{
					DisplaySegment(Grid.GetInner2DPoint(EGridSpace::UniformScaled, IndexNode1) * Grid.DisplayScale, Grid.GetInner2DPoint(EGridSpace::UniformScaled, IndexNode2) * Grid.DisplayScale, IndexNode1, EVisuProperty::GreenCurve);
				}
#endif
			}
		}
		else
		{
#ifdef DEBUG_BUILDINNERSEGMENTS
			if (bDisplay)
			{
				DisplaySegment(Grid.GetInner2DPoint(EGridSpace::UniformScaled, IndexNode1) * Grid.DisplayScale, Grid.GetInner2DPoint(EGridSpace::UniformScaled, IndexNode2) * Grid.DisplayScale, IndexNode1, EVisuProperty::RedCurve);
			}
#endif
		}

		FIsoInnerNode& Node1 = *GlobalIndexToIsoInnerNodes[IndexNode1];
		FIsoInnerNode& Node2 = *GlobalIndexToIsoInnerNodes[IndexNode2];
		FIsoSegment& Segment = IsoSegmentFactory.New();
		Segment.Init(Node1, Node2, InType);
		Segment.ConnectToNode();
		FinalInnerSegments.Add(&Segment);
	};

	for (int32 uIndex = 0; uIndex < NumU; uIndex++)
	{
		for (int32 vIndex = 0; vIndex < NumV - 1; vIndex++)
		{
			BuildSegmentIfValid(Grid.GobalIndex(uIndex, vIndex), Grid.GobalIndex(uIndex, vIndex + 1), ESegmentType::IsoV);
		}
	}

	for (int32 vIndex = 0; vIndex < NumV; vIndex++)
	{
		for (int32 uIndex = 0; uIndex < NumU - 1; uIndex++)
		{
			BuildSegmentIfValid(Grid.GobalIndex(uIndex, vIndex), Grid.GobalIndex(uIndex + 1, vIndex), ESegmentType::IsoU);
		}
	}

#ifdef CADKERNEL_DEV
	Chronos.BuildInnerSegmentsDuration += FChrono::Elapse(StartTime);
#endif
}

void FIsoTriangulator::BuildInnerSegmentsIntersectionTool()
{
	FTimePoint StartTime = FChrono::Now();

	// Find Boundary Segments Of Inner Triangulation
	// 
	// A pixel grid is build. 
	// A pixel is the quadrangle of the inner grid
	// The grid pixel are initialized to False
	//
	// A pixel is True if one of its boundary segment does not exist 
	// The inner of the grid is all pixel False
	// The boundary of the inner triangulation is defined by all segments adjacent to different cells 
	// 
	//    T      T	     T                                                  
	//       0 ----- 0 												     0 ----- 0 
	//    T  |   F   |   T       T       T      T         			     |       |    
	//       0 ----- 0               0 ----- 0 						     0       0               0 ----- 0 
	//    T  |   F   |   T       T   |   F   |  T					     |       |               |       |  
	//       0 ----- 0 ----- 0 ----- 0 ----- 0 						     0       0 ----- 0 ----- 0       0 	
	//    T  |   F   |   F   |   F   |   F   |	T					     |                               |	
	//       0 ----- 0 ----- 0 ----- 0 ----- 0 						     0                               0 	
	//    T  |   F   |   F   |   F   |   F   |	T					     |                               |	
	//       0 ----- 0 ----- 0 ----- 0 ----- 0 						     0 ----- 0 ----- 0 ----- 0 ----- 0 	
	//    T      T 		 T		 T		 T		T					                
	// 
	// https://docs.google.com/presentation/d/1qUVOH-2kU_QXBVKyRUcdDy1Y6WGkcaJCiaS8wGjSZ6M/edit?usp=sharing
	// Slide "Boundary Segments Of Inner Triangulation"

	int32 NumU = Grid.GetCuttingCount(EIso::IsoU);
	int32 NumV = Grid.GetCuttingCount(EIso::IsoV);

	TArray<uint8> Pixel;
	Pixel.Init(0, Grid.GetTotalCuttingCount());

#ifdef DEBUG_FINDBOUNDARYSEGMENTS
	if (bDisplay)
	{
		F3DDebugSession _(TEXT("FIsoTrianguler::InnerSegment"));
		for (int32 IndexV = 0, Index = 0; IndexV < NumV; ++IndexV)
		{
			for (int32 IndexU = 0; IndexU < NumU; ++IndexU, ++Index)
			{
				if (!Grid.IsNodeInsideFace(Index))
				{
					continue;
				}

				if (GlobalIndexToIsoInnerNodes[Index]->IsLinkedToNextU())
				{
					Grid.DisplayIsoSegment(EGridSpace::UniformScaled, *GlobalIndexToIsoInnerNodes[Index], *GlobalIndexToIsoInnerNodes[Index + 1], Index, EVisuProperty::YellowCurve);
				}

				if (GlobalIndexToIsoInnerNodes[Index]->IsLinkedToPreviousU())
				{
					Grid.DisplayIsoSegment(EGridSpace::UniformScaled, *GlobalIndexToIsoInnerNodes[Index], *GlobalIndexToIsoInnerNodes[Index - 1], Index, EVisuProperty::YellowCurve);
				}

				if (GlobalIndexToIsoInnerNodes[Index]->IsLinkedToNextV())
				{
					Grid.DisplayIsoSegment(EGridSpace::UniformScaled, *GlobalIndexToIsoInnerNodes[Index], *GlobalIndexToIsoInnerNodes[Index + NumU], Index, EVisuProperty::YellowCurve);
				}

				if (GlobalIndexToIsoInnerNodes[Index]->IsLinkedToPreviousV())
				{
					Grid.DisplayIsoSegment(EGridSpace::UniformScaled, *GlobalIndexToIsoInnerNodes[Index], *GlobalIndexToIsoInnerNodes[Index - NumU], Index, EVisuProperty::YellowCurve);
				}
			}
		}
	}
#endif

	// A pixel is True if one of its boundary segment does not exist 
	for (int32 IndexV = 0, Index = 0; IndexV < NumV; ++IndexV)
	{
		for (int32 IndexU = 0; IndexU < NumU; ++IndexU, ++Index)
		{
			if (!Grid.IsNodeInsideFace(Index))
			{
				continue;
			}

			if (!GlobalIndexToIsoInnerNodes[Index]->IsLinkedToNextU())
			{
				Pixel[Index] = true;
				Pixel[Index - NumU] = true;
			}

			if (!GlobalIndexToIsoInnerNodes[Index]->IsLinkedToPreviousU())
			{
				Pixel[Index - 1] = true;
				Pixel[Index - 1 - NumU] = true;
			}

			if (!GlobalIndexToIsoInnerNodes[Index]->IsLinkedToNextV())
			{
				Pixel[Index] = true;
				Pixel[Index - 1] = true;
			}

			if (!GlobalIndexToIsoInnerNodes[Index]->IsLinkedToPreviousV())
			{
				Pixel[Index - NumU] = true;
				Pixel[Index - NumU - 1] = true;
			}
		}
	}

	// The boundary of the inner triangulation is defined by all segments adjacent to a "True" cell 
	// These segments are added to InnerSegmentsIntersectionTool
	InnerSegmentsIntersectionTool.Reserve((int32)FinalInnerSegments.Num());

	for (FIsoSegment* Segment : FinalInnerSegments)
	{
		int32 IndexFirstNode = Segment->GetFirstNode().GetIndex();
		int32 IndexSecondNode = 0;
		switch (Segment->GetType())
		{
		case ESegmentType::IsoU:
			IndexSecondNode = IndexFirstNode - NumU;
			break;
		case ESegmentType::IsoV:
			IndexSecondNode = IndexFirstNode - 1;
			break;
		default:
			ensureCADKernel(false);
		}
		if (Pixel[IndexFirstNode] || Pixel[IndexSecondNode])
		{
			InnerSegmentsIntersectionTool.AddSegment(*Segment);
		}
	}

	FindInnerGridCellSurroundingSmallLoop();

	// initialize the intersection tool
	InnerSegmentsIntersectionTool.Sort();

#ifdef CADKERNEL_DEV
	Chronos.FindLoopSegmentOfInnerTriangulationDuration += FChrono::Elapse(StartTime);
#endif

#ifdef DEBUG_FINDBOUNDARYSEGMENTS
	DisplayPixels(Pixel);
	//Wait(bDisplay);
#endif
}

void FIsoTriangulator::FindIsoSegmentToLinkLoopToLoop()
{
	FTimePoint StartTime = FChrono::Now();

	// This coefficient is to defined the tolerance on coordinates according the iso strip...
	// With some surface, the parameterization speed can vary enormously depending on the point of the surface.
	// A good information is the width of iso strip around a point. Indeed, strips have the optimal width to respect meshing criteria.
	// So a fraction of the strip's width defined a good tolerance around a given point. 
	constexpr double ToleranceCoefficent = 1. / 12.; // Why 12 ? ;o)

	const TArray<double>& IsoUCoordinates = Grid.GetUniformCuttingCoordinatesAlongIso(EIso::IsoU);
	const TArray<double>& IsoVCoordinates = Grid.GetUniformCuttingCoordinatesAlongIso(EIso::IsoV);


	// Warning, Min delta is compute in EGridSpace::Uniform
	TFunction<double(const TArray<double>&)> GetMinDelta = [](const TArray<double>& IsoCoordinates)
	{
		double MinDelta = HUGE_VAL;
		for (int32 Index = 0; Index < IsoCoordinates.Num() - 1; ++Index)
		{
			double Delta = IsoCoordinates[Index + 1] - IsoCoordinates[Index];
			if (Delta < MinDelta)
			{
				MinDelta = Delta;
			}
		}
		return MinDelta;
	};

	// Before creating a segment a set of check is done to verify that the segment is valid.
	TFunction<void(FLoopNode*, const FPoint2D&, FLoopNode*, const FPoint2D&)> CreateSegment = [&](FLoopNode* Node1, const FPoint2D& Coordinate1, FLoopNode* Node2, const FPoint2D& Coordinate2)
	{
		if (&Node1->GetPreviousNode() == Node2 || &Node1->GetNextNode() == Node2)
		{
			return;
		}

		if (Node1->GetSegmentConnectedTo(Node2))
		{
			return;
		}

		// Is Outside and not too flat at Node1
		ensureCADKernel(Node1->GetLoopIndex() > 0);
		const double FlatAngle = 0.1;
		if (Node1->IsSegmentBeInsideFace(Coordinate2, Grid, FlatAngle))
		{
			return;
		}

		// Is Outside and not too flat at Node2
		ensureCADKernel(Node2->GetLoopIndex() > 0);
		if (Node2->IsSegmentBeInsideFace(Coordinate1, Grid, FlatAngle))
		{
			return;
		}

		if (InnerSegmentsIntersectionTool.DoesIntersect(Coordinate1, Coordinate2))
		{
			return;
		}

		if (LoopSegmentsIntersectionTool.DoesIntersect(*Node1, *Node2))
		{
			return;
		}

		FIsoSegment& Segment = IsoSegmentFactory.New();
		Segment.Init(*Node1, *Node2, ESegmentType::LoopToLoop);
		Segment.ConnectToNode();
		FinalToLoops.Add(&Segment);
		InnerToLoopSegmentsIntersectionTool.AddSegment(Segment);
#ifdef DEBUG_FIND_ISOSEGMENT_TO_LINK_LOOP_TO_LOOP
		Display(EGridSpace::UniformScaled, *Node1, *Node2);
#endif
	};

	// Find the index of the closed Iso strip. An iso strip is a the strip [Iso[Index], Iso[Index+1]]
	// As the process is iterative with sorted points, Index can only be equal or bigger than with the previous node
	TFunction<void(const TArray<double>&, int32&, const double&)> FindStripIndex = [&](const TArray<double>& Iso, int32& Index, const double& PointCoord)
	{
		if (Index > 0)
		{
			--Index;
		}

		// The last strip is not tested as it must be good if the previous are not even if PointCoord >= Iso.Last
		for (; Index < Iso.Num() - 2; ++Index)
		{
			if (PointCoord < Iso[Index + 1])
			{
				break;
			}
		}
	};

	TArray<FLoopNode*> SortedLoopNodesAlong = SortedLoopNodes;

	// Find pair of points iso aligned along Axe2
	// For all loop nodes sorted along Axe1, check if the pair (Node[i], Node[i+1]) is aligned along Axe2. 
	// The segment (Node[i], Node[i+1]) is valid if its length is smaller or nearly equal to a crossing strip. 
	// Axe1 == 0 => IsoU, coordinate U is ~constant
	// Axe1 == 1 => IsoV

	TFunction<void(int32, const TArray<double>&, const TArray<double>&)> FindIsoSegmentAlong = [&](int32 InAxe, const TArray<double>& IsoU, const TArray<double>& IsoV)
	{
		EIso ComplementaryAxe = (InAxe + 1) % 2 == 0 ? EIso::IsoU : EIso::IsoV;

		int32 IndexU = 0;
		for (int32 Index = 0; Index < SortedLoopNodesAlong.Num() - 1; ++Index)
		{
			FLoopNode* LoopNode = SortedLoopNodesAlong[Index];
			if (!LoopNode->IsIso(ComplementaryAxe))
			{
				continue;
			}
			FLoopNode* NextNode = SortedLoopNodesAlong[Index + 1];
			if (!NextNode->IsIso(ComplementaryAxe))
			{
				continue;
			}

			const FPoint2D& LoopPoint = LoopNode->Get2DPoint(EGridSpace::UniformScaled, Grid);

#ifdef DEBUG_FIND_ISOSEGMENT_TO_LINK_LOOP_TO_LOOP
			F3DDebugSession G(FString::Printf(TEXT("Iso %d Index %d"), InAxe, Index));
			Display(EGridSpace::UniformScaled, *LoopNode);
#endif

			FindStripIndex(IsoU, IndexU, LoopPoint[InAxe]);

			double ToleranceU = (IsoU[IndexU + 1] - IsoU[IndexU]) * ToleranceCoefficent;

#ifdef DEBUG_FIND_ISOSEGMENT_TO_LINK_LOOP_TO_LOOP
			Display(EGridSpace::UniformScaled, *NextNode, 0, EVisuProperty::RedPoint);
#endif
			const FPoint2D& NextPoint = NextNode->Get2DPoint(EGridSpace::UniformScaled, Grid);
			if (FMath::IsNearlyEqual(NextPoint[InAxe], LoopPoint[InAxe], ToleranceU))
			{
				// the nodes are nearly iso aligned, are they nearly in the same V Stip
				double MinV = LoopPoint[ComplementaryAxe];
				double MaxV = NextPoint[ComplementaryAxe];
				GetMinMax(MinV, MaxV);

				int32 IndexV = 0;
				FindStripIndex(IsoV, IndexV, MinV);

				if (IndexV >= IsoV.Num() - 1)
				{
					continue;
				}

				// We check that segment length is not greater the the crossing strip width
				bool bIsSmallerThanStrip = false;
				if (MaxV <= IsoV[IndexV + 1])
				{
					// both point are in the same strip
					bIsSmallerThanStrip = true;
				}
				else
				{
					// either MinV is nearly equal to IsoV[IndexV + 1]- 
					double FirstStripCrossingLength = (IsoV[IndexV + 1] - MinV) / (IsoV[IndexV + 1] - IsoV[IndexV]);
					if (IndexV < IsoV.Num() - 2 && MaxV < IsoV[IndexV + 1])
					{
						double SecondStripCrossingLength = (MaxV - IsoV[IndexV + 1]) / (IsoV[IndexV + 2] - IsoV[IndexV + 1]);
						if ((FirstStripCrossingLength + SecondStripCrossingLength) < 1 + ToleranceCoefficent)
						{
							bIsSmallerThanStrip = true;
						}
					}
					// either MaxV is nearly equal to IsoV[IndexV + 1]+
					else if (IndexV < IsoV.Num() - 3 && MaxV < IsoV[IndexV + 2])
					{
						double ThirdStripCrossingLength = (MaxV - IsoV[IndexV + 2]) / (IsoV[IndexV + 3] - IsoV[IndexV + 2]);
						if ((FirstStripCrossingLength + ThirdStripCrossingLength) < ToleranceCoefficent)
						{
							bIsSmallerThanStrip = true;
						}
					}
				}
				if (bIsSmallerThanStrip)
				{
					CreateSegment(LoopNode, LoopPoint, NextNode, NextPoint);
				}
			}
		}
	};

	int32 InitNum = InnerSegmentsIntersectionTool.Count() + LoopSegmentsIntersectionTool.Count();
	FinalToLoops.Reserve(InitNum);
	InnerToLoopSegmentsIntersectionTool.Reserve(InitNum);

	// Nodes are sorted according to a value function of their coordinates.
	// To sort along U, the value is U + DeltaFactor*(V - VMin)
	// DeltaFactor is a value that for all values Ui of U, Ui + DeltaFactor.(VMax - VMin) < U(i+1)
	// With this, Node[i+1] is either the next node of the same side of the loop, either the closed U aligned node of the opposite loop.  
	{
		constexpr int32 IsoU = 0; // coordinate U is ~constant
		constexpr int32 IsoV = 1; // coordinate V is ~constant

		double DeltaFactor = FMath::Min((GetMinDelta(IsoUCoordinates) / 1000.), (GetMinDelta(IsoVCoordinates) / 1000.));

		// Bounds and GetMinDelta are defined in EGridSpace::Default2D,
		//const FSurfacicBoundary& Bounds = Grid.GetFace()->GetBoundary();
		double UMin = Grid.GetUniformCuttingCoordinates()[EIso::IsoU][0]; // Bounds.UVBoundaries[EIso::IsoU].Min;
		double VMin = Grid.GetUniformCuttingCoordinates()[EIso::IsoV][0]; // Bounds.UVBoundaries[EIso::IsoV].Min;
		Algo::Sort(SortedLoopNodesAlong, [&](const FLoopNode* LoopNode1, const FLoopNode* LoopNode2)
			{
				const FPoint2D& Node1Coordinates = LoopNode1->Get2DPoint(EGridSpace::UniformScaled, Grid);
				const FPoint2D& Node2Coordinates = LoopNode2->Get2DPoint(EGridSpace::UniformScaled, Grid);
				return (Node1Coordinates.U + (Node1Coordinates.V - VMin) * DeltaFactor) < (Node2Coordinates.U + (Node2Coordinates.V - VMin) * DeltaFactor);
			});
		FindIsoSegmentAlong(IsoU, IsoUCoordinates, IsoVCoordinates);

		Algo::Sort(SortedLoopNodesAlong, [&](const FLoopNode* LoopNode1, const FLoopNode* LoopNode2)
			{
				const FPoint2D& Node1Coordinates = LoopNode1->Get2DPoint(EGridSpace::UniformScaled, Grid);
				const FPoint2D& Node2Coordinates = LoopNode2->Get2DPoint(EGridSpace::UniformScaled, Grid);
				return (Node1Coordinates.V + (Node1Coordinates.U - UMin) * DeltaFactor) < (Node2Coordinates.V + (Node2Coordinates.U - UMin) * DeltaFactor);
			});
		FindIsoSegmentAlong(IsoV, IsoVCoordinates, IsoUCoordinates);
	}
#ifdef CADKERNEL_DEV
	Chronos.FindInnerSegmentToLinkLoopToLoopDuration += FChrono::Elapse(StartTime);
#endif
}

void FIsoTriangulator::FindSegmentToLinkOuterLoopNodes(FCell& Cell)
{
	int SubdivisionCount = Cell.OuterLoopSubdivision.Num();
	for (int32 Andex = 0; Andex < SubdivisionCount - 1; ++Andex)
	{
		TArray<FLoopNode*>& SubLoopA = Cell.OuterLoopSubdivision[Andex];
		for (int32 Bndex = Andex + 1; Bndex < SubdivisionCount; ++Bndex)
		{
			TArray<FLoopNode*>& SubLoopB = Cell.OuterLoopSubdivision[Bndex];
			TryToConnectTwoSubLoopsWithShortestSegment(Cell, SubLoopA, SubLoopB);
		}
	}
	Cell.SelectSegmentInCandidateSegments(IsoSegmentFactory);
}

void FIsoTriangulator::FindSegmentToLinkOuterToInnerLoopNodes(FCell& Cell)
{
	TArray<FLoopNode*>& OuterLoop = Cell.SubLoops[0];

	TFunction<void(FLoopNode&, FLoopNode&)> CreateCandidateSegment = [&](FLoopNode& Node, FLoopNode& CandidatNode)
	{
		FIsoSegment& Segment = IsoSegmentFactory.New();
		Segment.Init(Node, CandidatNode, ESegmentType::LoopToLoop);
		Segment.SetCandidate();
		Cell.CandidateSegments.Add(&Segment);

#ifdef DEBUG_FIND_SEGMENT_TO_LINK_OUTER_TO_INNER_LOOP
		if (bDisplay)
		{
			{
				F3DDebugSession _(FString::Printf(TEXT("Candidate Segment")));
				Grid.DisplayIsoSegment(EGridSpace::UniformScaled, Node, CandidatNode, 0, EVisuProperty::RedCurve);
			}
		}
#endif 
	};

	const SlopMethod GetSlopAtStartNode = CounterClockwiseSlop;
	const SlopMethod GetSlopAtEndNode = ClockwiseSlop;
	TFunction<void(FIsoSegment*)> FindBestTriangle = [&](FIsoSegment* Segment)
	{
		FLoopNode& StartNode = (FLoopNode&)Segment->GetFirstNode();
		FLoopNode& EndNode = (FLoopNode&)Segment->GetSecondNode();

		if ((StartNode.GetConnectedSegments().Num() > 2) || (EndNode.GetConnectedSegments().Num() > 2))
		{
			return;
		}

		FIsoSegment& PreviousSegment = StartNode.GetPreviousSegment();
		FIsoSegment& NextSegment = EndNode.GetNextSegment();

		FIsoNode& PreviousNode = StartNode.GetPreviousNode();
		FIsoNode& NextNode = EndNode.GetNextNode();

#ifdef DEBUG_FIND_SEGMENT_TO_LINK_OUTER_TO_INNER_LOOP
		static int32 SegmentIndex = 0;
		if (bDisplay)
		{
			{
				SegmentIndex++;
				F3DDebugSession _(FString::Printf(TEXT("Start Segment %d"), SegmentIndex));
				Grid.DisplayIsoSegment(EGridSpace::UniformScaled, *Segment, SegmentIndex, EVisuProperty::BlueCurve);
				Grid.DisplayIsoSegment(EGridSpace::UniformScaled, PreviousSegment, SegmentIndex, EVisuProperty::GreenCurve);
				Grid.DisplayIsoSegment(EGridSpace::UniformScaled, NextSegment, SegmentIndex, EVisuProperty::GreenCurve);
			}
		}
#endif

		const FPoint2D& StartPoint2D = StartNode.Get2DPoint(EGridSpace::UniformScaled, Grid);
		const FPoint2D& EndPoint2D = EndNode.Get2DPoint(EGridSpace::UniformScaled, Grid);

		const FPoint& StartPoint3D = StartNode.Get3DPoint(Grid);
		const FPoint& EndPoint3D = EndNode.Get3DPoint(Grid);

		// StartMaxSlope and EndMaxSlope are at most equal to 4, because if the slop with candidate node is biggest to 4, the nez triangle will be inverted
		double StartReferenceSlope = ComputePositiveSlope(StartPoint2D, EndPoint2D, 0);

		double StartMaxSlope = GetSlopAtStartNode(StartPoint2D, PreviousNode.Get2DPoint(EGridSpace::UniformScaled, Grid), StartReferenceSlope);
		StartMaxSlope /= 2.;
		StartMaxSlope = FMath::Min(StartMaxSlope, 4.);

		double EndReferenceSlope = StartReferenceSlope < 4 ? StartReferenceSlope + 4 : StartReferenceSlope - 4;
		double EndMaxSlope = GetSlopAtEndNode(EndPoint2D, NextNode.Get2DPoint(EGridSpace::UniformScaled, Grid), EndReferenceSlope);
		EndMaxSlope /= 2.;
		EndMaxSlope = FMath::Min(EndMaxSlope, 4.);

		const double MinSlopToNotBeAligned = 0.01;

		FLoopNode* CandidatNode = nullptr;
		double MinCriteria = HUGE_VALUE;
		double CandidateSlopeAtStartNode = 8.;
		double CandidateSlopeAtEndNode = 8.;

		for (int32 Index = 1; Index < Cell.SubLoops.Num(); ++Index)
		{
			for (FLoopNode* Node : Cell.SubLoops[Index])
			{
				// Check if the node is inside the sector (X) or outside (Z)
				const FPoint2D& NodePoint2D = Node->Get2DPoint(EGridSpace::UniformScaled, Grid);
				double SlopeAtStartNode = GetSlopAtStartNode(StartPoint2D, NodePoint2D, StartReferenceSlope);
				double SlopeAtEndNode = GetSlopAtEndNode(EndPoint2D, NodePoint2D, EndReferenceSlope);

				if (SlopeAtStartNode <= 0 || SlopeAtStartNode >= StartMaxSlope)
				{
					continue;
				}

				if (SlopeAtEndNode <= 0 || SlopeAtEndNode >= EndMaxSlope)
				{
					continue;
				}

				double PointCriteria = IsoTriangulatorImpl::IsoscelesCriteria(StartPoint2D, EndPoint2D, NodePoint2D);

				if (
					// the candidate triangle is inside the current candidate triangle
					((SlopeAtStartNode < (CandidateSlopeAtStartNode + MinSlopToNotBeAligned)) && (SlopeAtEndNode < (CandidateSlopeAtEndNode + MinSlopToNotBeAligned))) ||
					// or the candidate triangle is better the current candidate triangle and doesn't contain the current candidate triangle
					((PointCriteria < MinCriteria) && ((SlopeAtStartNode > CandidateSlopeAtStartNode) ^ (SlopeAtEndNode > CandidateSlopeAtEndNode))))
				{

					if (LoopSegmentsIntersectionTool.DoesIntersect(StartNode, *Node))
					{
						continue;
					}

					if (LoopSegmentsIntersectionTool.DoesIntersect(EndNode, *Node))
					{
						continue;
					}

					MinCriteria = PointCriteria;
					CandidatNode = Node;
					CandidateSlopeAtStartNode = SlopeAtStartNode;
					CandidateSlopeAtEndNode = SlopeAtEndNode;
				}
			}
		}

		if (CandidatNode)
		{
			CreateCandidateSegment(StartNode, *CandidatNode);
			CreateCandidateSegment(EndNode, *CandidatNode);
		}
	};

	for (int32 Index = 0; Index < OuterLoop.Num() - 1; ++Index)
	{
		FLoopNode* Node = OuterLoop[Index];
		if (&Node->GetNextNode() != OuterLoop[Index + 1])
		{
			continue;
		}

		FIsoSegment* Segment = Node->GetSegmentConnectedTo(OuterLoop[Index + 1]);
		if (Segment == nullptr)
		{
			continue;
		}
		FindBestTriangle(Segment);
	}

	Cell.SelectSegmentInCandidateSegments(IsoSegmentFactory);
}

//#define DEBUG_FIND_ISO_SEGMENT_TO_LINK_OUTER_LOOP_NODES
void FIsoTriangulator::FindIsoSegmentToLinkOuterLoopNodes(FCell& Cell)
{
	TArray<FLoopNode*>& OuterLoop = Cell.SubLoops[0];
	int32 NodeCount = OuterLoop.Num();

	{
#ifdef DEBUG_FIND_ISO_SEGMENT_TO_LINK_OUTER_LOOP_NODES
		F3DDebugSession _(bDisplay, ("FindIsoSegmentToLinkOuterLoopNodes 1"));
#endif
		int SubdivisionCount = Cell.OuterLoopSubdivision.Num();
		for (int32 Andex = 0; Andex < SubdivisionCount - 1; ++Andex)
		{
			TArray<FLoopNode*>& SubLoopA = Cell.OuterLoopSubdivision[Andex];
			for (int32 Bndex = Andex + 1; Bndex < SubdivisionCount; ++Bndex)
			{
				TArray<FLoopNode*>& SubLoopB = Cell.OuterLoopSubdivision[Bndex];
				TryToConnectTwoSubLoopsWithTheMostIsoSegment(Cell, SubLoopA, SubLoopB);
			}
		}
	}

	{
#ifdef DEBUG_FIND_ISO_SEGMENT_TO_LINK_OUTER_LOOP_NODES
		F3DDebugSession _(bDisplay, ("FindIsoSegmentToLinkOuterLoopNodes 2"));
#endif
		for (TArray<FLoopNode*>& SubLoop : Cell.OuterLoopSubdivision)
		{
			TryToConnectVertexSubLoopWithTheMostIsoSegment(Cell, SubLoop);
		}
	}
	
	Cell.SelectSegmentInCandidateSegments(IsoSegmentFactory);
}

// =============================================================================================================
// 	   For each cell
// 	      - Connect loops together and to cell vertices
// 	           - Find subset of node of each loop
// 	           - build Delaunay connection
// 	           - find the shortest segment to connect each connected loop by Delaunay
// =============================================================================================================
//#define DEBUG_CONNECT_CELL_LOOPS
void FIsoTriangulator::ConnectCellLoops()
{
	TArray<FCell> Cells;
	FindCellContainingBoundaryNodes(Cells);

#ifdef DEBUG_CONNECT_CELL_LOOPS
	F3DDebugSession _(bDisplay, ("ConnectCellLoops"));
#endif

	FinalToLoops.Reserve(LoopNodeCount + InnerNodeCount);
	for (FCell& Cell : Cells)
	{
#ifdef DEBUG_CONNECT_CELL_LOOPS
		F3DDebugSession _(bDisplay, FString::Printf(TEXT("Cell %d"), Cell.Id));
		DisplayCell(Cell);
#endif

		ConnectCellSubLoopsByNeighborhood(Cell);

#ifdef DEBUG_CONNECT_CELL_LOOPS
		Grid.DisplayIsoSegments(TEXT("ConnectCellLoops::Step 0"), EGridSpace::UniformScaled, Cell.FinalSegments, false, false, EVisuProperty::YellowCurve);
#endif

		if (Cell.bHasOuterLoop)
		{
			FindIsoSegmentToLinkOuterLoopNodes(Cell);
#ifdef DEBUG_CONNECT_CELL_LOOPS
			Grid.DisplayIsoSegments(TEXT("ConnectCellLoops::Step 1"), EGridSpace::UniformScaled, Cell.FinalSegments, false, false, EVisuProperty::YellowCurve);
#endif

			if (Cell.CandidateSegments.Num() == 0)
			{
				FindSegmentToLinkOuterLoopNodes(Cell);
#ifdef DEBUG_CONNECT_CELL_LOOPS
				Grid.DisplayIsoSegments(TEXT("ConnectCellLoops::Step 2"), EGridSpace::UniformScaled, Cell.FinalSegments, false, false, EVisuProperty::YellowCurve);
#endif

				FindSegmentToLinkOuterToInnerLoopNodes(Cell);
#ifdef DEBUG_CONNECT_CELL_LOOPS
				Grid.DisplayIsoSegments(TEXT("ConnectCellLoops::Step 3"), EGridSpace::UniformScaled, Cell.FinalSegments, false, false, EVisuProperty::YellowCurve);
#endif
			}
		}
		ConnectCellCornerToInnerLoop(Cell);
#ifdef DEBUG_CONNECT_CELL_LOOPS
		Grid.DisplayIsoSegments(TEXT("ConnectCellLoops::Step 4"), EGridSpace::UniformScaled, Cell.FinalSegments, false, false, EVisuProperty::YellowCurve);
#endif

		FinalToLoops.Append(Cell.FinalSegments);

#ifdef DEBUG_CONNECT_CELL_LOOPS
		Grid.DisplayIsoSegments(TEXT("ConnectCellLoops::Final Iso ToLink Inner To Loops"), EGridSpace::UniformScaled, FinalToLoops, false, false, EVisuProperty::YellowCurve);
#endif
	}
}

//#define DEBUG_BUILD_CELLS
void FIsoTriangulator::FindCellContainingBoundaryNodes(TArray<FCell>& Cells)
{
	FTimePoint StartTime = FChrono::Now();

	TArray<int32> NodeToCellIndices;
	TArray<int32> SortedIndex;

	const int32 CountU = Grid.GetCuttingCount(EIso::IsoU);
	const int32 CountV = Grid.GetCuttingCount(EIso::IsoV);
	const int32 MaxUV = Grid.GetTotalCuttingCount();

	const TArray<double>& IsoUCoordinates = Grid.GetUniformCuttingCoordinatesAlongIso(EIso::IsoU);
	const TArray<double>& IsoVCoordinates = Grid.GetUniformCuttingCoordinatesAlongIso(EIso::IsoV);

	NodeToCellIndices.Reserve(LoopNodeCount);
	{
		int32 IndexU = 0;
		int32 IndexV = 0;
		int32 Index = 0;
		int32 DeletedNodeCount = 0;

#ifdef DEBUG_BUILD_CELLS
		F3DDebugSession _(bDisplay, TEXT("Debug Find index"));
#endif

		for (const FLoopNode& LoopPoint : LoopNodes)
		{
			if (!LoopPoint.IsDelete())
			{
				const FPoint2D& Coordinate = LoopPoint.Get2DPoint(EGridSpace::UniformScaled, Grid);
				ArrayUtils::FindCoordinateIndex(IsoUCoordinates, Coordinate.U, IndexU);
				ArrayUtils::FindCoordinateIndex(IsoVCoordinates, Coordinate.V, IndexV);

#ifdef DEBUG_BUILD_CELLS
				if (bDisplay)
				{
					F3DDebugSession S(FString::Printf(TEXT("Index %d"), IndexV * CountU + IndexU));
					DisplayPoint(LoopPoint.GetPoint(EGridSpace::UniformScaled, Grid), EVisuProperty::BluePoint);
					DrawCellBoundary(IndexV * CountU + IndexU, EVisuProperty::YellowPoint);
				}
#endif

				NodeToCellIndices.Emplace(IndexV * CountU + IndexU);
			}
			else
			{
#ifdef DEBUG_BUILD_CELLS
				if (bDisplay)
				{
					F3DDebugSession S(TEXT("Deleted Node"));
					DisplayPoint(LoopPoint.GetPoint(EGridSpace::UniformScaled, Grid), EVisuProperty::RedPoint);
				}
#endif

				DeletedNodeCount++;
				NodeToCellIndices.Emplace(MaxUV);
			}
			SortedIndex.Emplace(Index++);
		}

		Algo::Sort(SortedIndex, [&](const int32& Index1, const int32& Index2)
			{
				return (NodeToCellIndices[Index1] < NodeToCellIndices[Index2]);
			});

		SortedIndex.SetNum(SortedIndex.Num() - DeletedNodeCount);
	}

	int32 CountOfCellsFilled = 1;
	{
		int32 CellIndex = NodeToCellIndices[0];
		for (int32 Index : SortedIndex)
		{
			if (CellIndex != NodeToCellIndices[Index])
			{
				CellIndex = NodeToCellIndices[Index];
				CountOfCellsFilled++;
			}
		}
	}

	// build Cells
	{
		Cells.Reserve(CountOfCellsFilled);
		int32 CellIndex = NodeToCellIndices[SortedIndex[0]];
		TArray<FLoopNode*> CellNodes;
		CellNodes.Reserve(LoopNodeCount);

#ifdef DEBUG_BUILD_CELLS
		F3DDebugSession _(bDisplay, TEXT("Build Cells"));
#endif

		for (int32 Index : SortedIndex)
		{
			if (CellIndex != NodeToCellIndices[Index])
			{
				Cells.Emplace(CellIndex, CellNodes, Grid);

				CellIndex = NodeToCellIndices[Index];
				CellNodes.Reset(LoopNodeCount);

#ifdef DEBUG_BUILD_CELLS
				DisplayCell(Cells.Last());
#endif
			}

			FLoopNode& LoopNode = LoopNodes[Index];
			if (!LoopNode.IsDelete())
			{
				CellNodes.Add(&LoopNode);
			}
		}
		Cells.Emplace(CellIndex, CellNodes, Grid);

#ifdef DEBUG_BUILD_CELLS
		DisplayCell(Cells.Last());
		Wait(bDisplay);
#endif
	}
	FChrono::Elapse(StartTime);
}

void FIsoTriangulator::FindCandidateSegmentsToLinkInnerAndLoop()
{
	const double FlatAngle = 0.1;

#ifdef CADKERNEL_DEV
	FTimePoint StartTime = FChrono::Now();
#endif
	TFunction<void(FIsoInnerNode*, FLoopNode&)> CreateCandidateSegment = [&](FIsoNode* InnerNode, FLoopNode& LoopNode)
	{
		FIsoSegment& SegCandidate = IsoSegmentFactory.New();
		SegCandidate.Init(*InnerNode, LoopNode, ESegmentType::InnerToLoop);
		NewTestSegments.Add(&SegCandidate);
	};

	TFunction<void(FLoopNode&, FLoopNode&)> CreateCandidateBoundarySegment = [&](FLoopNode& StartNode, FLoopNode& EndNode)
	{
		FIsoSegment& SegCandidate = IsoSegmentFactory.New();
		SegCandidate.Init(StartNode, EndNode, ESegmentType::LoopToLoop);
		NewTestSegments.Add(&SegCandidate);
	};

	int32 CountU = Grid.GetCuttingCount(EIso::IsoU);
	int32 CountV = Grid.GetCuttingCount(EIso::IsoV);

	// find cell containing boundary nodes
	TArray<int32> NodeToCellIndices;
	TArray<int32> SortedIndex;
	{
		//F3DDebugSession _(TEXT("New Test"));

		const TArray<double>& IsoUCoordinates = Grid.GetUniformCuttingCoordinatesAlongIso(EIso::IsoU);
		const TArray<double>& IsoVCoordinates = Grid.GetUniformCuttingCoordinatesAlongIso(EIso::IsoV);

		NodeToCellIndices.Reserve(LoopNodeCount);
		int32 IndexU = 0;
		int32 IndexV = 0;
		int32 Index = 0;
		for (const FLoopNode& LoopNode : LoopNodes)
		{
			if (LoopNode.IsDelete())
			{
				continue;
			}

			const FPoint2D& Coordinate = LoopNode.Get2DPoint(EGridSpace::UniformScaled, Grid);

			ArrayUtils::FindCoordinateIndex(IsoUCoordinates, Coordinate.U, IndexU);
			ArrayUtils::FindCoordinateIndex(IsoVCoordinates, Coordinate.V, IndexV);

			NodeToCellIndices.Emplace(IndexV * CountU + IndexU);
			SortedIndex.Emplace(Index++);
		}

		Algo::Sort(SortedIndex, [&](const int32& Index1, const int32& Index2)
			{
				return (NodeToCellIndices[Index1] < NodeToCellIndices[Index2]);
			});
	}


	int32 CellIndex = -1;

	FIsoInnerNode* Cell[4] = { nullptr, nullptr, nullptr, nullptr };
	TFunction<void(int32 CellIndex)> GetCellIsoNode = [&](int32 CellIndex)
	{
		int32 Index = CellIndex;
		Cell[0] = GlobalIndexToIsoInnerNodes[Index++];
		Cell[1] = GlobalIndexToIsoInnerNodes[Index];
		Index += CountU;
		Cell[2] = GlobalIndexToIsoInnerNodes[Index--];
		Cell[3] = GlobalIndexToIsoInnerNodes[Index];
	};

	// create segment between a boundary node and a cell border
	//Open3DDebugSession(FString::Printf(TEXT("Cell %d"), CellIndex));
	for (int32 Index : SortedIndex)
	{
		if (CellIndex != NodeToCellIndices[Index])
		{
			CellIndex = NodeToCellIndices[Index];
			GetCellIsoNode(CellIndex);

			//Close3DDebugSession();
			//Open3DDebugSession(FString::Printf(TEXT("Cell %d"), CellIndex));
		}

		FLoopNode& LoopPoint = LoopNodes[Index];

		for (int32 ICell = 0; ICell < 4; ++ICell)
		{
			if (Cell[ICell])
			{
				if (LoopPoint.IsSegmentBeInsideFace(Cell[ICell]->Get2DPoint(EGridSpace::UniformScaled, Grid), Grid, FlatAngle))
				{
					continue;
				}

				if (LoopSegmentsIntersectionTool.DoesIntersect(*Cell[ICell], LoopPoint))
				{
					continue;
				}

				CreateCandidateSegment(Cell[ICell], LoopPoint);
			}
		}

		//DisplayPoint(LoopPoint.Get2DPoint(EGridSpace::UniformScaled, Grid), EVisuProperty::YellowPoint);
	}
	//Close3DDebugSession();

	// create segment between two boundary nodes
	CellIndex = -1;
	//Open3DDebugSession(TEXT("Find in Cell "));
	//Open3DDebugSession(FString::Printf(TEXT("Cell %d"), 0));
	for (int32 Index = 0; Index < SortedIndex.Num() - 1; ++Index)
	{
		int32 ISortedIndex = SortedIndex[Index];
		FLoopNode& StartLoop = LoopNodes[ISortedIndex];
		const FPoint2D& StartPoint = StartLoop.Get2DPoint(EGridSpace::UniformScaled, Grid);

		//if (CellIndex != NodeToCellIndices[ISortedIndex])
		//{
		//	Close3DDebugSession();
		//	Open3DDebugSession(FString::Printf(TEXT("Cell %d"), NodeToCellIndices[ISortedIndex]));
		//	DisplayPoint(StartPoint, EVisuProperty::YellowPoint);
		//}

		CellIndex = NodeToCellIndices[ISortedIndex];

		for (int32 Jndex = Index + 1; Jndex < SortedIndex.Num(); ++Jndex)
		{
			int32 JSortedIndex = SortedIndex[Jndex];
			if (CellIndex != NodeToCellIndices[JSortedIndex])
			{
				break;
			}
			FLoopNode& EndLoop = LoopNodes[JSortedIndex];
			//DisplayPoint(EndLoop.Get2DPoint(EGridSpace::UniformScaled, Grid), EVisuProperty::YellowPoint);

			if (&EndLoop.GetPreviousNode() == &StartLoop || &EndLoop.GetNextNode() == &StartLoop)
			{
				continue;
			}

			const FPoint2D& EndPoint = EndLoop.Get2DPoint(EGridSpace::UniformScaled, Grid);

			if (StartLoop.IsSegmentBeInsideFace(EndPoint, Grid, FlatAngle))
			{
				continue;
			}

			if (EndLoop.IsSegmentBeInsideFace(StartPoint, Grid, FlatAngle))
			{
				continue;
			}

			if (LoopSegmentsIntersectionTool.DoesIntersect(StartLoop, EndLoop))
			{
				continue;
			}

			CreateCandidateBoundarySegment(StartLoop, EndLoop);
		}
	}
	//Close3DDebugSession();
	//Close3DDebugSession();
	//Wait();

	//Display(EGridSpace::UniformScaled, TEXT("TEST"), NewTestSegments, false);
	//Wait(true);

#ifdef CADKERNEL_DEV
	Chronos.FindSegmentToLinkInnerToLoopDuration = FChrono::Elapse(StartTime);
#endif
}

void FIsoTriangulator::ConnectUnconnectedInnerSegments()
{
	TArray<FIsoNode*> UnconnectedNode;
	UnconnectedNode.Reserve(FinalInnerSegments.Num());
	for (FIsoSegment* Segment : FinalInnerSegments)
	{
		if (Segment->GetFirstNode().GetConnectedSegments().Num() == 1)
		{
			UnconnectedNode.Add(&Segment->GetFirstNode());
		}
	}

	for (FIsoNode* Node : UnconnectedNode)
	{
		double MinDistance = HUGE_VALUE;
		FLoopNode* Candidate = nullptr;
		for (FLoopNode& LoopNode : LoopNodes)
		{
			if (LoopNode.IsDelete())
			{
				continue;
			}

			double Distance = LoopNode.Get2DPoint(EGridSpace::Scaled, Grid).SquareDistance(Node->Get2DPoint(EGridSpace::Scaled, Grid));
			if (Distance < MinDistance)
			{
				if (!InnerToLoopSegmentsIntersectionTool.DoesIntersect(*Node, LoopNode))
				{
					MinDistance = Distance;
					Candidate = &LoopNode;
				}
			}
		}

		if (Candidate)
		{
			FIsoSegment& Segment = IsoSegmentFactory.New();
			Segment.Init(*Node, *Candidate, ESegmentType::InnerToLoop);
			Segment.ConnectToNode();
			FinalToLoops.Add(&Segment);
			InnerToLoopSegmentsIntersectionTool.AddSegment(Segment);
			InnerToLoopSegmentsIntersectionTool.Sort();
		}
	}
}

bool FIsoTriangulator::CanCycleBeMeshed(const TArray<FIsoSegment*>& Cycle, FIntersectionSegmentTool& CycleIntersectionTool)
{
	bool bHasSelfIntersection = true;

	for (const FIsoSegment* Segment : Cycle)
	{
		if (CycleIntersectionTool.DoesIntersect(*Segment))
		{
			FMessage::Printf(Log, TEXT("A cycle of the surface %d is in self intersecting. The mesh of this sector is canceled.\n"), Grid.GetFace().GetId());

#ifdef DEBUG_CAN_CYCLE_BE_FIXED_AND_MESHED
			if (bDisplay)
			{
				F3DDebugSession _(TEXT("New Segment"));
				F3DDebugSession A(TEXT("Segments"));
				Display(EGridSpace::UniformScaled, *Segment, 0, EVisuProperty::RedPoint);
				Display(EGridSpace::UniformScaled, *IntersectingSegment, 0, EVisuProperty::BluePoint);
				Wait();
			}
#endif
			return false;
		}
	}

	return true;
}

#ifdef WIP_ADD_STEP_TO_TO_FAVOR_ISO_SEGMENTS
//#define DEBUG_FIND_ISO_SEGMENT_CYCLE
void FIsoTriangulator::FindIsoCandidateSegmentInCycle(TArray<FIsoNode*> CycleNodes)
{
#ifdef DEBUG_FIND_ISO_SEGMENT_CYCLE
	F3DDebugSession _(bDisplay, TEXT("FindIsoCandidateSegmentInCycle"));
#endif

	const double FlatSlope = 0.10; // ~5 deg: The segment must make an angle less than 10 deg with the Iso
	double MinSlope = FlatSlope + DOUBLE_SMALL_NUMBER; //.25; // ~10 deg: The segment must make an angle less than 10 deg with the Iso

	FIsoNode* CandidateA = nullptr;
	FIsoNode* CandidateB = nullptr;

	int32 CycleCount = CycleNodes.Num();

	TArray<FIsoSegment*> CandiateSegment;
	CandiateSegment.Reserve(CycleCount);

	for (int32 IndexA = 0; IndexA < CycleCount - 2; ++IndexA)
	{
		FIsoNode* NodeA = CycleNodes[IndexA];
		const FPoint2D& ACoordinates = NodeA->Get2DPoint(EGridSpace::UniformScaled, Grid);
		Grid.DisplayIsoNode(UniformScaled, *NodeA, 0, EVisuProperty::GreenPoint);

		for (int32 IndexB = IndexA + 2; IndexB < CycleCount; ++IndexB)
		{
			FIsoNode* NodeB = CycleNodes[IndexB];

			if(NodeA->GetSegmentConnectedTo(NodeB))
			{
				continue;
			}

			const FPoint2D& BCoordinates = NodeB->Get2DPoint(EGridSpace::UniformScaled, Grid);

			double Slope = ComputeSlopeRelativeToNearestAxis(ACoordinates, BCoordinates);
			if (Slope < MinSlope)
			{
				MinSlope = Slope;
				CandidateA = NodeA;
				CandidateB = NodeB;
			}
		}

		if (MinSlope < FlatSlope)
		{
			FIsoSegment& Segment = IsoSegmentFactory.New();
			Segment.Init(*CandidateA, *CandidateB, ESegmentType::LoopToLoop);
			Segment.SetCandidate();

			CandiateSegment.Add(&Segment);

#ifdef DEBUG_FIND_ISO_SEGMENT_CYCLE
			Grid.DisplayIsoSegment(UniformScaled, Segment, 0, EVisuProperty::YellowCurve);
			Wait(bDisplay);
#endif
			MinSlope = FlatSlope + DOUBLE_SMALL_NUMBER;
		}
	}

	if(bDisplay)
	{
		Wait();
	}

	// find the best iso segments i.e. ones that generate smooth surface
	// check that the selected segments won't block the meshing i.e. a cycle segment cannot be meshed
	for(FIsoSegment* Segment : CandiateSegment)
	{
	}

}
#endif


void FIsoTriangulator::MeshCycle(const EGridSpace Space, const TArray<FIsoSegment*>& Cycle, const TArray<bool>& CycleOrientation)
{
	int32 NodeCycleNum = (int32)Cycle.Num();

	if (NodeCycleNum == 4)
	{
		FIsoNode* Nodes[4];
		if (CycleOrientation[0])
		{
			Nodes[0] = &Cycle[0]->GetFirstNode();
			Nodes[1] = &Cycle[0]->GetSecondNode();
		}
		else
		{
			Nodes[0] = &Cycle[0]->GetSecondNode();
			Nodes[1] = &Cycle[0]->GetFirstNode();
		}

		if (CycleOrientation[2])
		{
			Nodes[2] = &Cycle[2]->GetFirstNode();
			Nodes[3] = &Cycle[2]->GetSecondNode();
		}
		else
		{
			Nodes[2] = &Cycle[2]->GetSecondNode();
			Nodes[3] = &Cycle[2]->GetFirstNode();
		}

		const FPoint2D* NodeCoordinates[4];
		for (int32 Index = 0; Index < 4; ++Index)
		{
			NodeCoordinates[Index] = &Nodes[Index]->Get2DPoint(Space, Grid);
		}

		double SegmentSlopes[4];
		SegmentSlopes[0] = ComputeSlope(*NodeCoordinates[0], *NodeCoordinates[1]);
		SegmentSlopes[1] = ComputeSlope(*NodeCoordinates[1], *NodeCoordinates[2]);
		SegmentSlopes[2] = ComputeSlope(*NodeCoordinates[2], *NodeCoordinates[3]);
		SegmentSlopes[3] = ComputeSlope(*NodeCoordinates[3], *NodeCoordinates[0]);

		double RelativeSlopes[4];
		RelativeSlopes[0] = TransformIntoOrientedSlope(SegmentSlopes[1] - SegmentSlopes[0]);
		RelativeSlopes[1] = TransformIntoOrientedSlope(SegmentSlopes[2] - SegmentSlopes[1]);
		RelativeSlopes[2] = TransformIntoOrientedSlope(SegmentSlopes[3] - SegmentSlopes[2]);
		RelativeSlopes[3] = TransformIntoOrientedSlope(SegmentSlopes[0] - SegmentSlopes[3]);

		int32 FlattenNodeIndex = 0;
		for (int32 IndexAngle = 0; IndexAngle < 4; ++IndexAngle)
		{
			if (RelativeSlopes[IndexAngle] < RelativeSlopes[FlattenNodeIndex])
			{
				FlattenNodeIndex = IndexAngle;
			}
		}

		int32 NodeIndices[4];
		NodeIndices[0] = FlattenNodeIndex;
		for (int32 IndexN = 1; IndexN < 4; ++IndexN)
		{
			NodeIndices[IndexN] = NodeIndices[IndexN - 1] == 3 ? 0 : NodeIndices[IndexN - 1] + 1;
		}

#ifdef ADD_TRIANGLE_2D
		if (bDisplay)
		{
			F3DDebugSession G(TEXT("Mesh cycle"));
			Grid.DisplayTriangle(EGridSpace::UniformScaled, *Nodes[NodeIndices[1]], *Nodes[NodeIndices[3]], *Nodes[NodeIndices[0]]);
			Grid.DisplayTriangle(EGridSpace::UniformScaled, *Nodes[NodeIndices[1]], *Nodes[NodeIndices[2]], *Nodes[NodeIndices[3]]);
		}
#endif 
		Mesh->AddTriangle(Nodes[NodeIndices[1]]->GetFaceIndex(), Nodes[NodeIndices[3]]->GetFaceIndex(), Nodes[NodeIndices[0]]->GetFaceIndex());
		Mesh->AddTriangle(Nodes[NodeIndices[1]]->GetFaceIndex(), Nodes[NodeIndices[2]]->GetFaceIndex(), Nodes[NodeIndices[3]]->GetFaceIndex());

		return;
	}
	else if (NodeCycleNum == 3)
	{
		if (CycleOrientation[0])
		{
#ifdef ADD_TRIANGLE_2D
			if (bDisplay)
			{
				F3DDebugSession G(TEXT("Mesh cycle"));
				Grid.DisplayTriangle(EGridSpace::UniformScaled, Cycle[0]->GetFirstNode(), Cycle[0]->GetSecondNode(), CycleOrientation[1] ? Cycle[1]->GetSecondNode() : Cycle[1]->GetFirstNode());
			}
#endif 
			Mesh->AddTriangle(Cycle[0]->GetFirstNode().GetFaceIndex(), Cycle[0]->GetSecondNode().GetFaceIndex(), CycleOrientation[1] ? Cycle[1]->GetSecondNode().GetFaceIndex() : Cycle[1]->GetFirstNode().GetFaceIndex());
		}
		else
		{
#ifdef ADD_TRIANGLE_2D
			if (bDisplay)
			{
				F3DDebugSession G(TEXT("Mesh cycle"));
				Grid.DisplayTriangle(EGridSpace::UniformScaled, Cycle[0]->GetSecondNode(), Cycle[0]->GetFirstNode(), CycleOrientation[1] ? Cycle[1]->GetSecondNode() : Cycle[1]->GetFirstNode());
			}
#endif 
			Mesh->AddTriangle(Cycle[0]->GetSecondNode().GetFaceIndex(), Cycle[0]->GetFirstNode().GetFaceIndex(), CycleOrientation[1] ? Cycle[1]->GetSecondNode().GetFaceIndex() : Cycle[1]->GetFirstNode().GetFaceIndex());
		}
		return;
	}

	const double SquareMinSize = FMath::Square(Grid.GetMinElementSize());

	FIntersectionSegmentTool CycleIntersectionTool(Grid);
	CycleIntersectionTool.Reserve(5 * NodeCycleNum);
	CycleIntersectionTool.AddSegments(Cycle);
	CycleIntersectionTool.Sort();

#ifdef ADD_TRIANGLE_2D
	static int32 CycleId = 0;
	F3DDebugSession _(bDisplay, FString::Printf(TEXT("Mesh cycle %d"), CycleId++));
	Grid.DisplayIsoSegments(TEXT("Cycle"), EGridSpace::UniformScaled, Cycle);
	Wait(bDisplay);
#endif

	// Check if the cycle is in self intersecting and fix it. 
	if (!CanCycleBeMeshed(Cycle, CycleIntersectionTool))
	{
#ifdef CADKERNEL_DEV
		MesherReport->Logs.AddCycleMeshingFailure();
#endif
		return;
	}

	TArray<FIsoNode*> CycleNodes;
	CycleNodes.Reserve(NodeCycleNum);

	TArray<FIsoSegment*> SegmentStack;
	SegmentStack.Reserve(5 * NodeCycleNum);

	{
		// Get cycle's nodes and set segments as they have a triangle outside the cycle (to don't try to mesh outside the cycle)
		auto SegmentOrientation = CycleOrientation.begin();
		for (auto Segment = Cycle.begin(); Segment != Cycle.end(); ++Segment, ++SegmentOrientation)
		{
			if (*SegmentOrientation)
			{
				CycleNodes.Add(&(*Segment)->GetFirstNode());
				ensureCADKernel(!(*Segment)->HasTriangleOnRight());
				(*Segment)->SetHasTriangleOnRight();
			}
			else
			{
				CycleNodes.Add(&(*Segment)->GetSecondNode());
				ensureCADKernel(!(*Segment)->HasTriangleOnLeft());
				(*Segment)->SetHasTriangleOnLeft();
			}
		}

		// If the Segment has 2 adjacent triangles, the segment is a inner cycle segment
		// It will have triangle in both side
		//
		//    X---------------X----------------X      X---------------X----------------X
		//    |                                |      |                                |  
		//    |         X--------------------X |      |                                |  
		//    |         |                    | |      |                                |  
		//    X---------X  <- inner segment  | |  or  X---------X  <- inner segment    |
		//    |         |                    | |      |                                |  
		//    |         X--------------------X |      |                                |  
		//    |                                |      |                                |
		//    X---------------X----------------X      X---------------X----------------X
		//
		for (FIsoSegment* Segment : Cycle)
		{
			if (Segment->HasTriangleOnRightAndLeft())
			{
				Segment->ResetHasTriangle();
			}

			if (Segment->GetFirstNode().GetConnectedSegments().Num() == 1 ||
				Segment->GetSecondNode().GetConnectedSegments().Num() == 1)
			{
				Segment->ResetHasTriangle();
			}
		}

		NodeCycleNum = (int32)Cycle.Num();

		TArray<int32> NodeIndex;
		NodeIndex.Reserve(NodeCycleNum);
		for (int32 Index = 0; Index < NodeCycleNum; ++Index)
		{
			NodeIndex.Add(Index);
		}

		TArray<double> SegmentLengths;
		SegmentLengths.Reserve(NodeCycleNum);
		for (int32 Index = 0, NextIndex = 1; Index < NodeCycleNum; ++Index, ++NextIndex)
		{
			if (NextIndex == NodeCycleNum)
			{
				NextIndex = 0;
			}

			double Length = CycleNodes[Index]->Get3DPoint(Grid).SquareDistance(CycleNodes[NextIndex]->Get3DPoint(Grid));
			if (Length < SquareMinSize)
			{
				Cycle[Index]->SetAsDegenerated();
			}
			SegmentLengths.Add(Length);
		}

		// Sort the nodes to process segments from the longest to the shortest 
		NodeIndex.Sort([SegmentLengths](const int32& Index1, const int32& Index2) { return SegmentLengths[Index1] > SegmentLengths[Index2]; });

		for (int32 Index = 0; Index < NodeCycleNum; ++Index)
		{
			SegmentStack.Add(Cycle[NodeIndex[Index]]);
		}
	}

	// Function used in FindBestTriangle
	TFunction<bool(FIsoNode*, FIsoNode*, FIsoSegment*)> BuildSegmentIfNeeded = [&](FIsoNode* NodeA, FIsoNode* NodeB, FIsoSegment* ABSegment) -> bool
	{
		if (ABSegment)
		{
			if (&ABSegment->GetFirstNode() == NodeA)
			{
				if (ABSegment->HasTriangleOnLeft())
				{
#ifdef ADD_TRIANGLE_2D
					F3DDebugSession _(FString::Printf(TEXT("Segment")));
					Grid.DisplayIsoSegment(EGridSpace::UniformScaled, *NodeA, *NodeB, 0, EVisuProperty::YellowCurve);
					Wait();
#endif
					return false;
				}
				ABSegment->SetHasTriangleOnLeft();
			}
			else
			{
				if (ABSegment->HasTriangleOnRight())
				{
#ifdef ADD_TRIANGLE_2D
					F3DDebugSession _(FString::Printf(TEXT("Segment")));
					Grid.DisplayIsoSegment(EGridSpace::UniformScaled, *NodeA, *NodeB, 0, EVisuProperty::YellowCurve);
					Wait();
#endif
					return false;
				}
				ABSegment->SetHasTriangleOnRight();
			}
		}
		else
		{
			FIsoSegment& NewSegment = IsoSegmentFactory.New();
			NewSegment.Init(*NodeA, *NodeB, ESegmentType::Unknown);
			NewSegment.ConnectToNode();
			CycleIntersectionTool.AddSegment(NewSegment);
			NewSegment.SetHasTriangleOnLeft();
			SegmentStack.Add(&NewSegment);
		}
		return true;
	};

	const SlopMethod GetSlopAtStartNode = ClockwiseSlop;
	const SlopMethod GetSlopAtEndNode = CounterClockwiseSlop;

	TFunction<void(FIsoSegment*, bool)> FindBestTriangle = [&](FIsoSegment* Segment, bool bOrientation)
	{
		// StartNode = A
		FIsoNode& StartNode = bOrientation ? Segment->GetFirstNode() : Segment->GetSecondNode();
		// EndNode = B
		FIsoNode& EndNode = bOrientation ? Segment->GetSecondNode() : Segment->GetFirstNode();


		//
		// For each extremity (A, B) of a segment, in the existing connected segments, the segment with the smallest relative slop is identified ([A, X0] and [B, Xn]).
		// These segments define the sector in which the best triangle could be.
		// The triangle to build is the best triangle (according to the Isosceles Criteria) connecting the Segment to one of the allowed nodes (X) between X0 and Xn.
		// Allowed nodes (X) are in the sector, Disallowed nodes (Z) are outside the sector	
		//
		//                                       ------Z------X0-------X------X-----X-------Xn----Z-----Z---
		//                                                     \                           /
		//                                                      \    Allowed triangles    /   
		//                                Not allowed triangles  \                       /   Not allowed triangles
		//                                                        \                     /
		//                                             ----Z-------A------Segment------B------Z---
		//
		//                                                         Not allowed triangles
		//
		// These computations are done in the UniformScaled space to avoid numerical error due to length distortion between U or V space and U or V Length.
		// i.e. if:
		// (UMax - UMin) / (VMax - VMin) is big 
		// and 
		// "medium length along U" / "medium length along V" is small 
		// The computed angles or slot is not representative of the 3D space.
		//
		// The computation is not done is Scale space to don't have problem with degenerated segments
		//
		// To avoid flat triangle, a candidate point must defined a minimal slop with [A, X0] or [B, Xn] to not be aligned with one of them. 
		//
		// TODO ? Need to add test to check if the candidate node will not generate flat triangle 

		// PreviousSegment = [A, X0]
		FIsoSegment* PreviousSegment = FindNextSegment(EGridSpace::UniformScaled, Segment, &StartNode, GetSlopAtStartNode);
		// NextSegment = [B, Xn]
		FIsoSegment* NextSegment = FindNextSegment(EGridSpace::UniformScaled, Segment, &EndNode, GetSlopAtEndNode);

		// PreviousNode = X0
		FIsoNode& PreviousNode = PreviousSegment->GetOtherNode(&StartNode);
		// NextNode = Xn
		FIsoNode& NextNode = NextSegment->GetOtherNode(&EndNode);

#ifdef DEBUG_FIND_BEST_TRIANGLE
		static int32 TriangleIndex = 0;
		if (bDisplay)
		{
			{
				TriangleIndex++;
				F3DDebugSession _(FString::Printf(TEXT("Start Segment %d %d"), TriangleIndex, bOrientation));
				Grid.DisplayIsoSegment(EGridSpace::UniformScaled, *Segment);
				Grid.DisplayIsoNode(EGridSpace::UniformScaled, StartNode, 0, EVisuProperty::RedPoint);
				Grid.DisplayIsoNode(EGridSpace::UniformScaled, EndNode);
			}

			{
				F3DDebugSession _(TEXT("Next Segments"));
				Grid.DisplayIsoSegment(EGridSpace::UniformScaled, *PreviousSegment);
				Grid.DisplayIsoSegment(EGridSpace::UniformScaled, *NextSegment, 0, EVisuProperty::BlueCurve);
				Grid.DisplayIsoNode(EGridSpace::UniformScaled, EndNode, 0, EVisuProperty::RedPoint);
				Grid.DisplayIsoNode(EGridSpace::UniformScaled, NextNode);
				Wait(TriangleIndex == 1000);
			}
		}
#endif

		FIsoNode* CandidatNode = nullptr;
		FIsoSegment* StartToCandiatSegment = nullptr;
		FIsoSegment* EndToCandiatSegment = nullptr;

		if (!NextSegment->IsDegenerated() && !PreviousSegment->IsDegenerated())
		{
			const FPoint2D& StartPoint2D = StartNode.Get2DPoint(EGridSpace::UniformScaled, Grid);
			const FPoint2D& EndPoint2D = EndNode.Get2DPoint(EGridSpace::UniformScaled, Grid);

			const FPoint& StartPoint3D = StartNode.Get3DPoint(Grid);
			const FPoint& EndPoint3D = EndNode.Get3DPoint(Grid);

			// StartMaxSlope and EndMaxSlope are at most equal to 4, because if the slop with candidate node is biggest to 4, the nez triangle will be inverted
			double StartReferenceSlope = ComputePositiveSlope(StartPoint2D, EndPoint2D, 0);
			double StartMaxSlope = GetSlopAtStartNode(StartPoint2D, PreviousNode.Get2DPoint(EGridSpace::UniformScaled, Grid), StartReferenceSlope);
			if (&EndNode != &PreviousNode)
			{
				// Case of probable auto-intersection cycle at PreviousNode, cancel is preferred 
				if (FMath::IsNearlyEqual(StartMaxSlope, 8., DOUBLE_KINDA_SMALL_NUMBER))
				{
					return;
				}
			}
			StartMaxSlope = FMath::Min(StartMaxSlope, 4.);

			double EndReferenceSlope = StartReferenceSlope < 4 ? StartReferenceSlope + 4 : StartReferenceSlope - 4;
			double EndMaxSlope = GetSlopAtEndNode(EndPoint2D, NextNode.Get2DPoint(EGridSpace::UniformScaled, Grid), EndReferenceSlope);
			if (&StartNode != &NextNode)
			{
				// Case of probable auto-intersection cycle at PreviousNode, cancel is preferred 
				if (FMath::IsNearlyEqual(EndMaxSlope, 8., DOUBLE_KINDA_SMALL_NUMBER))
				{
					return;
				}
			}
			EndMaxSlope = FMath::Min(EndMaxSlope, 4.);

			double MinCriteria = HUGE_VALUE;
			const double MinSlopToNotBeAligned = 0.01;
			const double EpsilonSlop = 0.0001;
			double CandidateSlopeAtStartNode = 8.;
			double CandidateSlopeAtEndNode = 8.;

			for (FIsoNode* Node : CycleNodes)
			{
				if (Node == &StartNode)
				{
					continue;
				}
				if (Node == &EndNode)
				{
					continue;
				}

				// Check if the node is inside the sector (X) or outside (Z)
				const FPoint2D& NodePoint2D = Node->Get2DPoint(EGridSpace::UniformScaled, Grid);
				double SlopeAtStartNode = GetSlopAtStartNode(StartPoint2D, NodePoint2D, StartReferenceSlope);
				double SlopeAtEndNode = GetSlopAtEndNode(EndPoint2D, NodePoint2D, EndReferenceSlope);

				if (Node != &PreviousNode)
				{
					if (SlopeAtStartNode <= EpsilonSlop || SlopeAtStartNode >= StartMaxSlope - EpsilonSlop)
					{
						continue;
					}
				}

				if (Node != &NextNode)
				{
					if (SlopeAtEndNode <= EpsilonSlop || SlopeAtEndNode >= EndMaxSlope - EpsilonSlop)
					{
						continue;
					}
				}

#ifdef DEBUG_FIND_BEST_TRIANGLE
				if (TriangleIndex == 14)
				{
					static int32 NodeIndex = 1;
					F3DDebugSession _(FString::Printf(TEXT("Next Node %d"), NodeIndex);
					Grid.DisplayIsoNode(EGridSpace::UniformScaled, *Node, ++NodeIndex, EVisuProperty::YellowPoint);
					Wait();
				}
#endif

				if (FMath::IsNearlyEqual(SlopeAtStartNode, CandidateSlopeAtStartNode, MinSlopToNotBeAligned) && SlopeAtEndNode > CandidateSlopeAtEndNode)
				{
					continue;
				}

				if (FMath::IsNearlyEqual(SlopeAtEndNode, CandidateSlopeAtEndNode, EpsilonSlop) && SlopeAtStartNode > CandidateSlopeAtStartNode)
				{
					continue;
				}

#ifdef D3_COTANGENT_CRITERIA
				// need more test, need to be tested with IsoscelesCriteria instead of 3D CotangentCriteria
				const FPoint& NodePoint3D = Node->Get3DPoint(Grid);
				FPoint NodeNormal;
				double PointCriteria = FMath::Abs(CotangentCriteria(StartPoint3D, EndPoint3D, NodePoint3D, NodeNormal));
				double CosAngle = FMath::Abs(ComputeCosinus(NodeNormal, Node->GetNormal(Grid)));

				// the criteria is weighted according to the cosine of the angle between the normal of the candidate triangle and the normal at the tested point
				if (CosAngle > DOUBLE_SMALL_NUMBER)
				{
					PointCriteria /= CosAngle;
				}
				else
				{
					PointCriteria = HUGE_VALUE;
				}
#else 
				double PointCriteria = IsoTriangulatorImpl::IsoscelesCriteria(StartPoint2D, EndPoint2D, NodePoint2D);
#endif
				if (
					// the candidate triangle is inside the current candidate triangle
					((SlopeAtStartNode < (CandidateSlopeAtStartNode + MinSlopToNotBeAligned)) && (SlopeAtEndNode < (CandidateSlopeAtEndNode + MinSlopToNotBeAligned)))
					||
					// the candidate triangle is better the current candidate triangle and doesn't contain the current candidate triangle
					((PointCriteria < MinCriteria) && ((SlopeAtStartNode > CandidateSlopeAtStartNode) ^ (SlopeAtEndNode > CandidateSlopeAtEndNode))))
				{
					// check if the candidate segment is not in intersection with existing segments
					// if the segment exist, it has already been tested
					FIsoSegment* StartSegment = StartNode.GetSegmentConnectedTo(Node);
					FIsoSegment* EndSegment = EndNode.GetSegmentConnectedTo(Node);

					if (!StartSegment && CycleIntersectionTool.DoesIntersect(StartNode, *Node))
					{
						continue;
					}

					if (!EndSegment && CycleIntersectionTool.DoesIntersect(EndNode, *Node))
					{
						continue;
					}

					MinCriteria = PointCriteria;
					CandidatNode = Node;
					StartToCandiatSegment = StartSegment;
					EndToCandiatSegment = EndSegment;
					CandidateSlopeAtStartNode = SlopeAtStartNode;
					CandidateSlopeAtEndNode = SlopeAtEndNode;
				}
			}
		}

		if (CandidatNode)
		{
			if (bOrientation)
			{
				Segment->SetHasTriangleOnRight();
			}
			else
			{
				Segment->SetHasTriangleOnLeft();
			}

			if (!BuildSegmentIfNeeded(&StartNode, CandidatNode, StartToCandiatSegment))
			{
				return;
			}
			if (!BuildSegmentIfNeeded(CandidatNode, &EndNode, EndToCandiatSegment))
			{
				return;
			}
			Mesh->AddTriangle(EndNode.GetFaceIndex(), StartNode.GetFaceIndex(), CandidatNode->GetFaceIndex());

#ifdef ADD_TRIANGLE_2D
			if (bDisplay)
			{
				{
					F3DDebugSession _(FString::Printf(TEXT("Triangle")));
					Grid.DisplayTriangle(EGridSpace::UniformScaled, EndNode, StartNode, *CandidatNode);
				}
			}
#endif 

			if (StartToCandiatSegment == nullptr || EndToCandiatSegment == nullptr)
			{
				CycleIntersectionTool.Sort();
			}
		}
	};

#ifdef WIP_ADD_STEP_TO_TO_FAVOR_ISO_SEGMENTS
	FindIsoCandidateSegmentInCycle(CycleNodes);
#endif



	for (int32 Index = 0; Index < SegmentStack.Num(); ++Index)
	{
		FIsoSegment* Segment = SegmentStack[Index];

		if (Segment->IsDegenerated())
		{
			continue;
		}
		if (!Segment->HasTriangleOnLeft())
		{
			FindBestTriangle(Segment, false);
		}
		if (!Segment->HasTriangleOnRight())
		{
			FindBestTriangle(Segment, true);
		}
	}

	// in case of incomplete cycle meshing, remove degenerated flags and rerun the process
	for (FIsoSegment* Segment : SegmentStack)
	{
		Segment->ResetDegenerated();
	}

	for (int32 Index = 0; Index < SegmentStack.Num(); ++Index)
	{
		FIsoSegment* Segment = SegmentStack[Index];

		if (!Segment->HasTriangleOnLeft())
		{
			FindBestTriangle(Segment, false);
		}
		if (!Segment->HasTriangleOnRight())
		{
			FindBestTriangle(Segment, true);
		}
	}

	// Reset the flags "has triangle" of cycle's segments to avoid to block the meshing of next cycles
	for (FIsoSegment* Segment : Cycle)
	{
		Segment->ResetHasTriangle();
	}
}

/**
 * The purpose is to add surrounding segments to the small loop to intersection tool to prevent traversing inner segments
 * A loop is inside inner segments
 *									|			 |
 *								   -----------------
 *									|	 XXX	 |
 *									|	XXXXX	 |
 *									|	 XXX	 |
 *								   -----------------
 *									|			 |
 *
 */
void FIsoTriangulator::FindInnerGridCellSurroundingSmallLoop()
{
	FTimePoint StartTime = FChrono::Now();

	if (GlobalIndexToIsoInnerNodes.Num() == 0)
	{
		// No inner node
		return;
	}

	// when an internal loop is inside inner UV cell
	int32 NumU = Grid.GetCuttingCount(EIso::IsoU);
	int32 NumV = Grid.GetCuttingCount(EIso::IsoV);
	const TArray<double>& UCoordinates = Grid.GetCuttingCoordinatesAlongIso(EIso::IsoU);
	const TArray<double>& VCoordinates = Grid.GetCuttingCoordinatesAlongIso(EIso::IsoV);

	const TArray<TArray<FPoint2D>>& Loops = Grid.GetLoops2D(EGridSpace::Default2D);
	for (int32 LoopIndex = 1; LoopIndex < Loops.Num(); ++LoopIndex)
	{
		FPoint2D FirstPoint = Loops[LoopIndex][0];

		int32 IndexU = 0;
		for (; IndexU < NumU - 1; ++IndexU)
		{
			if ((FirstPoint.U > UCoordinates[IndexU]) && (FirstPoint.U < UCoordinates[IndexU + 1] + DOUBLE_SMALL_NUMBER))
			{
				break;
			}
		}

		int32 IndexV = 0;
		for (; IndexV < NumV - 1; ++IndexV)
		{
			if ((FirstPoint.V > VCoordinates[IndexV]) && (FirstPoint.V < VCoordinates[IndexV + 1] + DOUBLE_SMALL_NUMBER))
			{
				break;
			}
		}

		double UMin = UCoordinates[IndexU];
		double UMax = UCoordinates[IndexU + 1] + DOUBLE_SMALL_NUMBER;
		double VMin = VCoordinates[IndexV];
		double VMax = VCoordinates[IndexV + 1] + DOUBLE_SMALL_NUMBER;

		bool bBoudardyIsSurrounded = true;
		for (const FPoint2D& LoopPoint : Loops[LoopIndex])
		{
			if (LoopPoint.U < UMin || LoopPoint.U > UMax || LoopPoint.V < VMin || LoopPoint.V > VMax)
			{
				bBoudardyIsSurrounded = false;
				break;
			}
		}

		if (bBoudardyIsSurrounded)
		{
			int32 Index = IndexV * NumU + IndexU;
			IndexOfLowerLeftInnerNodeSurroundingALoop.Add((int32)Index);

			FIsoInnerNode* Node = GlobalIndexToIsoInnerNodes[Index];
			if (Node == nullptr)
			{
				Node = GlobalIndexToIsoInnerNodes[Index + 1];
			}
			if (Node != nullptr)
			{
				for (FIsoSegment* Segment : Node->GetConnectedSegments())
				{
					if (Segment->GetType() == ESegmentType::IsoU)
					{
						if (Segment->GetSecondNode().GetIndex() == Index + 1)
						{
							InnerSegmentsIntersectionTool.AddSegment(*Segment);
						}
					}
					else if (Segment->GetSecondNode().GetIndex() == Index + NumU)
					{
						InnerSegmentsIntersectionTool.AddSegment(*Segment);
					}
				}
			}

			Index = (IndexV + 1) * NumU + IndexU + 1;
			Node = GlobalIndexToIsoInnerNodes[Index];
			if (Node == nullptr)
			{
				Node = GlobalIndexToIsoInnerNodes[Index - 1];
			}
			if (Node != nullptr)
			{
				for (FIsoSegment* Segment : Node->GetConnectedSegments())
				{
					if (Segment->GetType() == ESegmentType::IsoU)
					{
						if (Segment->GetFirstNode().GetIndex() == Index - 1)
						{
							InnerSegmentsIntersectionTool.AddSegment(*Segment);
						}
					}
					else if (Segment->GetFirstNode().GetIndex() == Index - NumU)
					{
						InnerSegmentsIntersectionTool.AddSegment(*Segment);
					}
				}
			}
		}
	}

#ifdef CADKERNEL_DEV
	Chronos.FindSegmentIsoUVSurroundingSmallLoopDuration += FChrono::Elapse(StartTime);
#endif
}

//#define FIND_CYCLE
void FIsoTriangulator::TriangulateOverCycle(const EGridSpace Space)
{
	FTimePoint StartTime = FChrono::Now();

	TArray<FIsoSegment*> Cycle;
	Cycle.Reserve(100);
	TArray<bool> CycleOrientation;
	CycleOrientation.Reserve(100);

	int32 CycleIndex = 0;
#ifdef FIND_CYCLE
	if (Grid.GetFace()->GetId() == FaceToDebug)
	{
		Open3DDebugSession(TEXT("Triangulate Over Cycle"));
	}
#endif

	for (FIsoSegment* Segment : LoopSegments)
	{
		if (!Segment->HasCycleOnLeft())
		{
			Cycle.Empty();
			CycleOrientation.Empty();
			bool bLeftSide = true;
			FindCycle(Segment, bLeftSide, Cycle, CycleOrientation);
#ifdef FIND_CYCLE
			if (Grid.GetFace()->GetId() == FaceToDebug)
			{
				F3DDebugSession G(TEXT("Find & mesh cycles"));
				FString Message = FString::Printf(TEXT("MeshCycle - cycle %d"), CycleIndex++);
				Display(EGridSpace::UniformScaled, *Message, Cycle, false);
			}
#endif
			MeshCycle(Space, Cycle, CycleOrientation);
		}
	}

	for (FIsoSegment* Segment : FinalToLoops)
	{
		if (!Segment->HasCycleOnLeft())
		{
			Cycle.Empty();
			CycleOrientation.Empty();
			bool bLeftSide = true;
			if (!FindCycle(Segment, bLeftSide, Cycle, CycleOrientation))
			{
				continue;
			}
#ifdef FIND_CYCLE
			if (Grid.GetFace()->GetId() == FaceToDebug)
			{
				Open3DDebugSession(TEXT("Find & mesh cycles"));
				FString Message = FString::Printf(TEXT("MeshCycle - cycle %d"), CycleIndex++);
				Display(EGridSpace::UniformScaled, *Message, Cycle, false);
				Close3DDebugSession();
			}
#endif
			MeshCycle(Space, Cycle, CycleOrientation);
		}

		if (!Segment->HasCycleOnRight())
		{
			Cycle.Empty();
			CycleOrientation.Empty();
			bool bLeftSide = false;
			if(!FindCycle(Segment, bLeftSide, Cycle, CycleOrientation))
			{
				continue;
			}
#ifdef FIND_CYCLE
			if (Grid.GetFace()->GetId() == FaceToDebug)
			{
				Open3DDebugSession(TEXT("Find & mesh cycles"));
				FString Message = FString::Printf(TEXT("MeshCycle - cycle %d"), CycleIndex++);
				Display(EGridSpace::UniformScaled, *Message, Cycle, false);
				Close3DDebugSession();
			}
#endif
			MeshCycle(Space, Cycle, CycleOrientation);
		}
	}
#ifdef FIND_CYCLE
	Close3DDebugSession();
#endif

#ifdef CADKERNEL_DEV
	Chronos.TriangulateOverCycleDuration = FChrono::Elapse(StartTime);
#endif
}

//#define DEBUG_FIND_CYCLE
#ifdef DEBUG_FIND_CYCLE
static int32 CycleId = -1;
static int32 CycleIndex = 0;
#endif

bool FIsoTriangulator::FindCycle(FIsoSegment* StartSegment, bool LeftSide, TArray<FIsoSegment*>& Cycle, TArray<bool>& CycleOrientation)
{

#ifdef DEBUG_FIND_CYCLE
	CycleIndex++;
	//CycleId = CycleIndex;
	if (Grid.GetFace()->GetId() == FaceToDebug)
	{
		CycleIndex = CycleId;
		bDisplay = true;
	}
#endif

	Cycle.Empty();
	CycleOrientation.Empty();

#ifdef DEBUG_FIND_CYCLE
	if (CycleId == CycleIndex)
	{
		Open3DDebugSession(TEXT("Cycle"));
	}
#endif

	FIsoSegment* Segment = StartSegment;
	FIsoNode* Node;

	if (LeftSide)
	{
		Segment->SetHaveCycleOnLeft();
		Node = &StartSegment->GetSecondNode();
#ifdef DEBUG_FIND_CYCLE
		if (CycleId == CycleIndex)
		{
			FIsoNode* EndNode = &StartSegment->GetFirstNode();
			F3DDebugSession _(TEXT("FirstSegment left"));
			Grid.DisplayIsoNode(EGridSpace::UniformScaled, *EndNode);
			Grid.DisplayIsoSegment(EGridSpace::UniformScaled, *StartSegment);
			//Wait();
		}
#endif
	}
	else
	{
		Segment->SetHaveCycleOnRight();
		Node = &StartSegment->GetFirstNode();
#ifdef DEBUG_FIND_CYCLE
		if (CycleId == CycleIndex)
		{
			FIsoNode* EndNode = &StartSegment->GetSecondNode();
			F3DDebugSession _(TEXT("FirstSegment right"));
			Grid.DisplayIsoNode(EGridSpace::UniformScaled, *EndNode);
			Grid.DisplayIsoSegment(EGridSpace::UniformScaled, *StartSegment);
			//Wait();
		}
#endif
	}

	Cycle.Add(StartSegment);
	CycleOrientation.Add(LeftSide);
	Segment = StartSegment;

	for (;;)
	{
		Segment = FindNextSegment(EGridSpace::UniformScaled, Segment, Node, ClockwiseSlop);
		if (Segment == nullptr)
		{
			Cycle.Empty();
			break;
		}

		if (Segment == StartSegment)
		{
			break;
		}

		Cycle.Add(Segment);

		if (&Segment->GetFirstNode() == Node)
		{
#ifdef DEBUG_HAS_CYCLE
			if (Segment->HasCycleOnLeft())
			{
				F3DDebugSession _(TEXT("Segment HasCycleOnLeft"));
				Grid.DisplayIsoSegment(EGridSpace::UniformScaled, *Segment, 0, EVisuProperty::RedPoint);
				Wait();
			}
#endif
			if (Segment->HasCycleOnLeft())
			{
				return false;
			}
			Segment->SetHaveCycleOnLeft();
			Node = &Segment->GetSecondNode();
			CycleOrientation.Add(true);
#ifdef DEBUG_FIND_CYCLE
			if (CycleId == CycleIndex)
			{
				F3DDebugSession _(TEXT("Next"));
				Grid.DisplayIsoNode(EGridSpace::UniformScaled, *Node);
				Grid.DisplayIsoSegment(EGridSpace::UniformScaled, *Segment);
				//Wait();
			}
#endif
		}
		else
		{
#ifdef DEBUG_HAS_CYCLE
			if (Segment->HasCycleOnRight())
			{
				F3DDebugSession _(TEXT("Segment HasCycleOnRight"));
				Grid.DisplayIsoSegment(EGridSpace::UniformScaled, *Segment, 0, EVisuProperty::RedPoint);
				Wait();
			}
#endif
			if (Segment->HasCycleOnRight())
			{
				return false;
			}
			Segment->SetHaveCycleOnRight();
			Node = &Segment->GetFirstNode();
			CycleOrientation.Add(false);
#ifdef DEBUG_FIND_CYCLE
			if (CycleId == CycleIndex)
			{
				F3DDebugSession _(TEXT("Next"));
				Grid.DisplayIsoNode(EGridSpace::UniformScaled, *Node);
				Grid.DisplayIsoSegment(EGridSpace::UniformScaled, *Segment);
				//Wait();
			}
#endif
		}
		if (false)
		{
			Wait();
		}
	}
#ifdef DEBUG_FIND_CYCLE
	if (CycleId == CycleIndex)
	{
		Close3DDebugSession();
	}
#endif
	return true;
}

//#define DEBUG_FIND_NEXTSEGMENT
#ifdef DEBUG_FIND_NEXTSEGMENT
static bool bDisplayStar = false;
#endif

FIsoSegment* FIsoTriangulator::FindNextSegment(EGridSpace Space, const FIsoSegment* StartSegment, const FIsoNode* StartNode, SlopMethod GetSlop) const
{
	const FPoint2D& StartPoint = StartNode->Get2DPoint(Space, Grid);
	const FPoint2D& EndPoint = (StartNode == &StartSegment->GetFirstNode()) ? StartSegment->GetSecondNode().Get2DPoint(Space, Grid) : StartSegment->GetFirstNode().Get2DPoint(Space, Grid);

#ifdef DEBUG_FIND_NEXTSEGMENT
	F3DDebugSession G(bDisplayStar, TEXT("FindNextSegment"));
	if (bDisplayStar)
	{
		F3DDebugSession _(TEXT("Start Segment"));
		Display(EGridSpace::Default2D, *StartNode);
		Display(EGridSpace::Default2D, *StartSegment);
		Display(Space, *StartNode);
		Display(Space, *StartSegment);
	}
#endif

	double ReferenceSlope = ComputePositiveSlope(StartPoint, EndPoint, 0);

	double MaxSlope = 8.1;
	FIsoSegment* NextSegment = nullptr;

	for (FIsoSegment* Segment : StartNode->GetConnectedSegments())
	{
		const FPoint2D& OtherPoint = (StartNode == &Segment->GetFirstNode()) ? Segment->GetSecondNode().Get2DPoint(Space, Grid) : Segment->GetFirstNode().Get2DPoint(Space, Grid);

		double Slope = GetSlop(StartPoint, OtherPoint, ReferenceSlope);
		if (Slope < SMALL_NUMBER_SQUARE) Slope = 8;

#ifdef DEBUG_FIND_NEXTSEGMENT
		if (bDisplayStar)
		{
			F3DDebugSession _(FString::Printf(TEXT("Segment slop %f"), Slope));
			Display(EGridSpace::Default2D, *Segment);
			Display(Space, *Segment);
		}
#endif

		if (Slope < MaxSlope || NextSegment == StartSegment)
		{
			NextSegment = Segment;
			MaxSlope = Slope;
		}
	}

	return NextSegment;
}

void FIsoTriangulator::TriangulateInnerNodes()
{
	FTimePoint StartTime = FChrono::Now();

	int32 NumU = Grid.GetCuttingCount(EIso::IsoU);
	int32 NumV = Grid.GetCuttingCount(EIso::IsoV);

#ifdef ADD_TRIANGLE_2D
	if (bDisplay)
	{
		Open3DDebugSession(TEXT("Inner Mesh 2D"));
	}
#endif
	for (int32 vIndex = 0, Index = 0; vIndex < NumV - 1; vIndex++)
	{
		for (int32 uIndex = 0; uIndex < NumU - 1; uIndex++, Index++)
		{
			// Do the lower nodes of the cell exist
			if (!GlobalIndexToIsoInnerNodes[Index] || !GlobalIndexToIsoInnerNodes[Index + 1])
			{
				continue;
			}

			// Is the lower left node connected
			if (!GlobalIndexToIsoInnerNodes[Index]->IsLinkedToNextU() || !GlobalIndexToIsoInnerNodes[Index]->IsLinkedToNextV())
			{
				continue;
			}

			// Do the upper nodes of the cell exist
			int32 OppositIndex = Index + NumU + 1;
			if (!GlobalIndexToIsoInnerNodes[OppositIndex] || !GlobalIndexToIsoInnerNodes[OppositIndex - 1])
			{
				continue;
			}

			// Is the top right node connected
			if (!GlobalIndexToIsoInnerNodes[OppositIndex]->IsLinkedToPreviousU() || !GlobalIndexToIsoInnerNodes[OppositIndex]->IsLinkedToPreviousV())
			{
				continue;
			}

			bool bIsSurroundingALoop = false;
			for (int32 BorderIndex : IndexOfLowerLeftInnerNodeSurroundingALoop)
			{
				if (Index == BorderIndex)
				{
					bIsSurroundingALoop = true;
					break;
				}
			}
			if (bIsSurroundingALoop)
			{
				continue;
			}

#ifdef ADD_TRIANGLE_2D
			Grid.DisplayTriangle(EGridSpace::UniformScaled, *GlobalIndexToIsoInnerNodes[Index], *GlobalIndexToIsoInnerNodes[Index + 1], *GlobalIndexToIsoInnerNodes[OppositIndex]);
			Grid.DisplayTriangle(EGridSpace::UniformScaled, *GlobalIndexToIsoInnerNodes[OppositIndex], *GlobalIndexToIsoInnerNodes[OppositIndex - 1], *GlobalIndexToIsoInnerNodes[Index]);
#endif 

			Mesh->AddTriangle(GlobalIndexToIsoInnerNodes[Index]->GetFaceIndex(), GlobalIndexToIsoInnerNodes[Index + 1]->GetFaceIndex(), GlobalIndexToIsoInnerNodes[OppositIndex]->GetFaceIndex());
			Mesh->AddTriangle(GlobalIndexToIsoInnerNodes[OppositIndex]->GetFaceIndex(), GlobalIndexToIsoInnerNodes[OppositIndex - 1]->GetFaceIndex(), GlobalIndexToIsoInnerNodes[Index]->GetFaceIndex());
		}
		Index++;
	}
#ifdef ADD_TRIANGLE_2D
	Close3DDebugSession();
#endif
}

//#define DEBUG_CONNECT_CELL_SUB_LOOPS_BY_NEIGHBORHOOD
void FIsoTriangulator::ConnectCellSubLoopsByNeighborhood(FCell& Cell)
{
	FTimePoint StartTime = FChrono::Now();

	int32 LoopCount = Cell.SubLoops.Num();

	TArray<TPair<int32,FPoint2D>> LoopBarycenters;
	LoopBarycenters.Reserve(LoopCount + 4);

	int32 LoopIndex = -1;
	for (const TArray<FLoopNode*>& Nodes : Cell.SubLoops)
	{
		++LoopIndex;

		// the external loop is not processed 
		if (Nodes[0]->GetLoopIndex() == 0)
		{
			continue;
		}

		TPair<int32, FPoint2D>& BaryCenterObj = LoopBarycenters.Emplace_GetRef(LoopIndex, FPoint2D::ZeroPoint);
		FPoint2D& BaryCenter = BaryCenterObj.Value;
		for (const FLoopNode* Node : Nodes)
		{
			BaryCenter += Node->Get2DPoint(EGridSpace::UniformScaled, Grid);
		}
		BaryCenter /= (double)Nodes.Num();
	}

	TArray<int32> EdgeVertexIndices;
	if (Cell.bHasOuterLoop && LoopCount < 5)
	{
		EdgeVertexIndices.Reserve(6);
		Cell.BorderLoopIndices.Reserve(3);
		if (LoopCount == 2)
		{
			Cell.BorderLoopIndices.Add(1);
		}
		else if (LoopCount == 3)
		{
			EdgeVertexIndices.Append({ 1, 2 });
			Cell.BorderLoopIndices.Append({ 1, 2 });
		}
		else if (LoopCount == 4)
		{
			EdgeVertexIndices.Append({ 1, 2, 2, 3, 3, 1 });
			Cell.BorderLoopIndices.Append({ 1, 2, 3 });
		}
	}
	else if (LoopBarycenters.Num() < 4)
	{
		EdgeVertexIndices.Reserve(6);
		Cell.BorderLoopIndices.Reserve(3);
		if (LoopCount == 1)
		{
			Cell.BorderLoopIndices.Add(0);
		}
		else if (LoopCount == 2)
		{
			EdgeVertexIndices.Append({ 0, 1 });
			Cell.BorderLoopIndices.Append({ 0, 1 });
		}
		else if (LoopCount == 3)
		{
			EdgeVertexIndices.Append({ 0, 1, 1, 2, 2, 0 });
			Cell.BorderLoopIndices.Append({ 0, 1, 2 });
		}
	}
	else
	{
#ifdef DEBUG_BOWYERWATSON
		FBowyerWatsonTriangulator::bDisplay = bDisplay;
#endif
		FBowyerWatsonTriangulator Triangulator(LoopBarycenters, EdgeVertexIndices);
		Triangulator.Triangulate();
		Triangulator.GetOuterVertices(Cell.BorderLoopIndices);
	}

	// Connect inner close loops 
	// ==========================================================================================
	{
#ifdef DEBUG_CONNECT_CELL_SUB_LOOPS_BY_NEIGHBORHOOD
		//F3DDebugSession _(TEXT("Build Segments Connect inner close loops "));
#endif

		for (int32 Index = 0; Index < EdgeVertexIndices.Num();)
		{
			int32 IndexLoopA = EdgeVertexIndices[Index++];
			int32 IndexLoopB = EdgeVertexIndices[Index++];

			const TArray<FLoopNode*>& SubLoopA = Cell.SubLoops[IndexLoopA];
			const TArray<FLoopNode*>& SubLoopB = Cell.SubLoops[IndexLoopB];

#ifdef DEBUG_CONNECT_CELL_SUB_LOOPS_BY_NEIGHBORHOOD
			//F3DDebugSession _(TEXT("Segment"));
#endif
			TryToConnectTwoSubLoopsWithShortestSegment(Cell, SubLoopA, SubLoopB);
		}

		Cell.SelectSegmentInCandidateSegments(IsoSegmentFactory);
#ifdef DEBUG_CONNECT_CELL_SUB_LOOPS_BY_NEIGHBORHOOD
		Display(EGridSpace::UniformScaled, TEXT("Build Segments Connect inner close loops"), Cell.FinalSegments, false, false, EVisuProperty::YellowCurve);
#endif
	}


	// With Outer loop
	// ==========================================================================================
	if (Cell.bHasOuterLoop && Cell.SubLoops.Num() > 1)
	{
#ifdef DEBUG_CONNECT_CELL_SUB_LOOPS_BY_NEIGHBORHOOD
		//F3DDebugSession _(TEXT("Build Segments to Outer loop"));
#endif
		for (TArray<FLoopNode*>& SubLoopA : Cell.OuterLoopSubdivision)
		{
			for (int32 IndexBorderLoop : Cell.BorderLoopIndices)
			{
				int32 CandidateSegmentsCount = Cell.CandidateSegments.Num();
				const TArray<FLoopNode*>& BorderLoop = Cell.SubLoops[IndexBorderLoop];

				TryToConnectTwoSubLoopsWithShortestSegment(Cell, SubLoopA, BorderLoop);
				if (CandidateSegmentsCount == Cell.CandidateSegments.Num())
				{
					// if the subloops have not been connnected with TryToConnectTwoSubLoopsWithShortestSegment
					// the try to connect them with an isoceles triangle
					// Loops must be connected together
					TryToConnectTwoLoopsWithIsocelesTriangle(Cell, SubLoopA, BorderLoop);
				}
			}
		}

		Cell.SelectSegmentInCandidateSegments(IsoSegmentFactory);
#ifdef DEBUG_CONNECT_CELL_SUB_LOOPS_BY_NEIGHBORHOOD
		Display(EGridSpace::UniformScaled, TEXT("Build Segments Connect inner to outer loop"), Cell.FinalSegments, false, false, EVisuProperty::YellowCurve);
#endif
	}


#ifdef CADKERNEL_DEV
	Chronos.FindSegmentToLinkLoopToLoopByDelaunayDuration += FChrono::Elapse(StartTime);
#endif
}

void FIsoTriangulator::TryToConnectTwoSubLoopsWithShortestSegment(FCell& Cell, const TArray<FLoopNode*>& LoopA, const TArray<FLoopNode*>& LoopB)
{
	double MinDistanceSquare = HUGE_VALUE_SQUARE;
	int32 MinIndexA = -1;
	int32 MinIndexB = -1;

	for (int32 IndexA = 0; IndexA < LoopA.Num(); ++IndexA)
	{
		const FLoopNode* NodeA = LoopA[IndexA];
		if (NodeA->IsDelete())
		{
			continue;
		}
		const FPoint2D& ACoordinates = NodeA->Get2DPoint(EGridSpace::UniformScaled, Grid);

		for (int32 IndexB = 0; IndexB < LoopB.Num(); ++IndexB)
		{
			const FLoopNode* NodeB = LoopB[IndexB];
			if (NodeB->IsDelete())
			{
				continue;
			}
			const FPoint2D& BCoordinates = NodeB->Get2DPoint(EGridSpace::UniformScaled, Grid);

			double SquareDistance = ACoordinates.SquareDistance(BCoordinates);
			if (SquareDistance < MinDistanceSquare)
			{
				MinDistanceSquare = SquareDistance;
				MinIndexA = IndexA;
				MinIndexB = IndexB;
			}
		}
	}

	if (MinIndexA >= 0 && MinIndexB >= 0)
	{
		FLoopNode* NodeA = LoopA[MinIndexA];
		const FPoint2D& ACoordinates = NodeA->Get2DPoint(EGridSpace::UniformScaled, Grid);
		FLoopNode* NodeB = LoopB[MinIndexB];
		const FPoint2D& BCoordinates = NodeB->Get2DPoint(EGridSpace::UniformScaled, Grid);

		//Display(EGridSpace::UniformScaled, *NodeA, *NodeB, 0, EVisuProperty::BlueCurve);
		if (TryToCreateSegment(Cell, NodeA, ACoordinates, NodeB, BCoordinates, 0.1))
		{
			NodeA = &LoopA[MinIndexA]->GetNextNode();
			if (Cell.Contains(NodeA))
			{
				NodeB = LoopB[MinIndexB];
				TryToCreateSegment(Cell, NodeA, NodeA->Get2DPoint(EGridSpace::UniformScaled, Grid), NodeB, NodeB->Get2DPoint(EGridSpace::UniformScaled, Grid), 0.1);
			}

			NodeB = &LoopB[MinIndexB]->GetNextNode();
			if (Cell.Contains(NodeB))
			{
				NodeA = LoopA[MinIndexA];
				TryToCreateSegment(Cell, NodeA, NodeA->Get2DPoint(EGridSpace::UniformScaled, Grid), NodeB, NodeB->Get2DPoint(EGridSpace::UniformScaled, Grid), 0.1);
			}
		}
	}

};

//#define DEBUG_TRY_TO_CONNECT
void FIsoTriangulator::TryToConnectTwoLoopsWithIsocelesTriangle(FCell& Cell, const TArray<FLoopNode*>& LoopA, const TArray<FLoopNode*>& LoopB)
{

	TFunction<FIsoNode* (FIsoSegment*)> FindBestTriangle = [&](FIsoSegment* Segment) -> FIsoNode*
	{
		SlopMethod GetSlopAtStartNode = ClockwiseSlop;
		SlopMethod GetSlopAtEndNode = CounterClockwiseSlop;

		// StartNode = A
		FIsoNode& StartNode = Segment->GetSecondNode();
		// EndNode = B
		FIsoNode& EndNode = Segment->GetFirstNode();

		//
		// For each segment of the LoopA, find in loopB a vertex that made the best triangle i.e. the triangle the most isosceles.
		// According to the knowledge of the orientation, only inside triangles are tested
		//
		// These computations are done in the UniformScaled space to avoid numerical error due to lenght distortion between U or V space and U or V Length.
		// i.e. if:
		// (UMax - UMin) / (VMax - VMin) is big 
		// and 
		// "medium length along U" / "medium length along V" is small 
		// 
		// To avoid flat triangle, a candidate point must defined a minimal slop with [A, X0] or [B, Xn] to not be aligned with one of them. 
		//

#ifdef DEBUG_FIND_BEST_TRIANGLE_TO_LINK_LOOPS
		int32 TriangleIndex = 0;
		if (bDisplay)
		{
			{
				TriangleIndex++;
				F3DDebugSession _(FString::Printf(TEXT("Start Segment %d"), TriangleIndex));
				Display(EGridSpace::UniformScaled, *Segment);
				Display(EGridSpace::UniformScaled, StartNode, 0, EVisuProperty::RedPoint);
				Display(EGridSpace::UniformScaled, EndNode);
			}
		}
#endif

		FIsoNode* CandidatNode = nullptr;
		FIsoSegment* StartToCandiatSegment = nullptr;
		FIsoSegment* EndToCandiatSegment = nullptr;

		const FPoint2D& StartPoint2D = StartNode.Get2DPoint(EGridSpace::UniformScaled, Grid);
		const FPoint2D& EndPoint2D = EndNode.Get2DPoint(EGridSpace::UniformScaled, Grid);

		double StartReferenceSlope = ComputePositiveSlope(StartPoint2D, EndPoint2D, 0);
		double EndReferenceSlope = StartReferenceSlope < 4 ? StartReferenceSlope + 4 : StartReferenceSlope - 4;

		double MinCriteria = HUGE_VALUE;
		const double MinSlopToNotBeAligned = 0.0001;
		double CandidateSlopeAtStartNode = 8.;
		double CandidateSlopeAtEndNode = 8.;

		for (FIsoNode* Node : LoopB)
		{
			// Check if the node is inside the sector (X) or outside (Z)
			const FPoint2D& NodePoint2D = Node->Get2DPoint(EGridSpace::UniformScaled, Grid);

			double PointCriteria = IsoTriangulatorImpl::IsoscelesCriteria(StartPoint2D, EndPoint2D, NodePoint2D);

			// Triangle that are too open (more than rectangle triangle) are not tested 
			if (PointCriteria > 0.6)
			{
				continue;
			}

			double SlopeAtStartNode = GetSlopAtStartNode(StartPoint2D, NodePoint2D, StartReferenceSlope);
			double SlopeAtEndNode = GetSlopAtEndNode(EndPoint2D, NodePoint2D, EndReferenceSlope);

			// check the side of the candidate point accordint to the segment
			if (SlopeAtStartNode <= MinSlopToNotBeAligned)
			{
				continue;
			}

			if (
				// the candidate triangle is inside the current candidate triangle
				((SlopeAtStartNode < (CandidateSlopeAtStartNode + MinSlopToNotBeAligned)) && (SlopeAtEndNode < (CandidateSlopeAtEndNode + MinSlopToNotBeAligned)))
				||
				// the candidate triangle is better than the current candidate triangle and doesn't contain the current candidate triangle
				((PointCriteria < MinCriteria) && ((SlopeAtStartNode > CandidateSlopeAtStartNode) ^ (SlopeAtEndNode > CandidateSlopeAtEndNode))))
			{
				// check if the candidate segment is not in intersection with existing segments
				// if the segment exist, it has already been tested
				FIsoSegment* StartSegment = StartNode.GetSegmentConnectedTo(Node);
				FIsoSegment* EndSegment = EndNode.GetSegmentConnectedTo(Node);

				if (!StartSegment && LoopSegmentsIntersectionTool.DoesIntersect(StartNode, *Node))
				{
					continue;
				}

				if (!EndSegment && LoopSegmentsIntersectionTool.DoesIntersect(EndNode, *Node))
				{
					continue;
				}

				MinCriteria = PointCriteria;
				CandidatNode = Node;
				StartToCandiatSegment = StartSegment;
				EndToCandiatSegment = EndSegment;
				CandidateSlopeAtStartNode = SlopeAtStartNode;
				CandidateSlopeAtEndNode = SlopeAtEndNode;
			}
		}

		return CandidatNode;
	};

	// for each segment of LoopA
	for (int32 IndexA = 0; IndexA < LoopA.Num() - 1; ++IndexA)
	{
		FLoopNode* NodeA1 = LoopA[IndexA];
		FLoopNode* NodeA2 = LoopA[IndexA + 1];
		const FPoint2D& A1Coordinates = NodeA1->Get2DPoint(EGridSpace::UniformScaled, Grid);
		const FPoint2D& A2Coordinates = NodeA2->Get2DPoint(EGridSpace::UniformScaled, Grid);

		FIsoSegment* Segment = NodeA1->GetSegmentConnectedTo(NodeA2);

		FIsoNode* Node = FindBestTriangle(Segment);
		if (Node)
		{
#ifdef DEBUG_TRY_TO_CONNECT
			if (bDisplay)
			{
				F3DDebugSession _(FString::Printf(TEXT("Triangle")));
				DisplayTriangle(EGridSpace::UniformScaled, *NodeA1, *NodeA2, *Node);
				Wait(true);

			}
#endif 
			const FPoint2D& NodeCoordinates = Node->Get2DPoint(EGridSpace::UniformScaled, Grid);
			TryToCreateSegment(Cell, NodeA1, A1Coordinates, Node, NodeCoordinates, 0.1);
			TryToCreateSegment(Cell, NodeA2, A2Coordinates, Node, NodeCoordinates, 0.1);
		}
	}

};

//#define DEBUG_FIND_ISO_SEGMENT_TO_LINK_OUTER_LOOP_NODES
void FIsoTriangulator::TryToConnectVertexSubLoopWithTheMostIsoSegment(FCell& Cell, const TArray<FLoopNode*>& Loop)
{
#ifdef DEBUG_FIND_ISO_SEGMENT_TO_LINK_OUTER_LOOP_NODES
	F3DDebugSession _(bDisplay, TEXT("TryToConnectVertexSubLoopWithTheMostIsoSegment"));
	if(bDisplay)
	{
		Wait();
	}
#endif

	const double FlatSlope = 0.10; // ~5 deg: The segment must make an angle less than 10 deg with the Iso
	double MinSlope = FlatSlope + DOUBLE_SMALL_NUMBER; //.25; // ~10 deg: The segment must make an angle less than 10 deg with the Iso


	if (Loop.Num() <= 2)
	{
		return;
	}

	int32 LoopCount = Loop.Num();
	for (int32 IndexA = 0; IndexA < LoopCount - 2; ++IndexA)
	{
		FLoopNode* CandidateB = nullptr;

		FLoopNode* CandidateA = Loop[IndexA];
		const FPoint2D& ACoordinates = CandidateA->Get2DPoint(EGridSpace::UniformScaled, Grid);

		FLoopNode* NextA = Loop[IndexA + 1];
		const FPoint2D& NextACoordinates = NextA->Get2DPoint(EGridSpace::UniformScaled, Grid);

		double ReferenceSlope = 0;
		{
			// slope SegmentA (NodeA, NextA)
			double Slope = ComputeUnorientedSlope(ACoordinates, NextACoordinates, 0);
			if (Slope > 1.5 && Slope < 2.5)
			{
				ReferenceSlope = 0;
			}
			else if ((Slope < 0.5) || (Slope > 3.5))
			{
				ReferenceSlope = 2;
			}
			else
			{
				// SegmentA is neither close to IsoV nor IsoU
				continue;
			}
		}

		for (int32 IndexB = IndexA + 2; IndexB < LoopCount; ++IndexB)
		{
			FLoopNode* NodeB = Loop[IndexB];
			const FPoint2D& BCoordinates = NodeB->Get2DPoint(EGridSpace::UniformScaled, Grid);

			double Slope = ComputeSlopeRelativeToReferenceAxis(ACoordinates, BCoordinates, ReferenceSlope);
			if (Slope < MinSlope)
			{
				MinSlope = Slope;
				CandidateB = NodeB;
			}
		}

		if (MinSlope < FlatSlope)
		{
			const FPoint2D& BCoordinates = CandidateB->Get2DPoint(EGridSpace::UniformScaled, Grid);

#ifdef DEBUG_FIND_ISO_SEGMENT_TO_LINK_OUTER_LOOP_NODES
			Grid.DisplayIsoSegment(UniformScaled, *CandidateA, *CandidateB, 0, EVisuProperty::RedCurve);
#endif
			TryToCreateSegment(Cell, CandidateA, ACoordinates, CandidateB, BCoordinates, 0.1);
			MinSlope = FlatSlope + DOUBLE_SMALL_NUMBER;
		}
	}

};

void FIsoTriangulator::TryToConnectTwoSubLoopsWithTheMostIsoSegment(FCell& Cell, const TArray<FLoopNode*>& LoopA, const TArray<FLoopNode*>& LoopB)
{
#ifdef DEBUG_FIND_ISO_SEGMENT_TO_LINK_OUTER_LOOP_NODES
	F3DDebugSession _(bDisplay, TEXT("TryToConnectVertexSubLoopWithTheMostIsoSegment"));
	if (bDisplay)
	{
		Wait();
	}
#endif

	const double FlatSlope = 0.10; // ~5 deg: The segment must make an angle less than 10 deg with the Iso
	double MinSlope = FlatSlope + DOUBLE_SMALL_NUMBER;// 0.25; // ~15 deg: The segment must make an angle less than 10 deg with the Iso

	for (FLoopNode* CandidateA : LoopA)
	{
		FLoopNode* CandidateB = nullptr;
		const FPoint2D& ACoordinates = CandidateA->Get2DPoint(EGridSpace::UniformScaled, Grid);

		for (FLoopNode* NodeB : LoopB)
		{
			const FPoint2D& BCoordinates = NodeB->Get2DPoint(EGridSpace::UniformScaled, Grid);

			double Slope = ComputeSlopeRelativeToNearestAxis(ACoordinates, BCoordinates);
			if (Slope < MinSlope)
			{
				MinSlope = Slope;
				CandidateB = NodeB;
			}
		}

		if (MinSlope < FlatSlope)
		{
			const FPoint2D& BCoordinates = CandidateB->Get2DPoint(EGridSpace::UniformScaled, Grid);

#ifdef DEBUG_FIND_ISO_SEGMENT_TO_LINK_OUTER_LOOP_NODES
			Grid.DisplayIsoSegment(EGridSpace::UniformScaled, *CandidateA, *CandidateB, 0, EVisuProperty::BlueCurve);
#endif			
			TryToCreateSegment(Cell, CandidateA, ACoordinates, CandidateB, BCoordinates, 0.1);
			MinSlope = FlatSlope + DOUBLE_SMALL_NUMBER;
		}
	}
}

//#define DEBUG_TRY_TO_CREATE_SEGMENT
bool FIsoTriangulator::TryToCreateSegment(FCell& Cell, FLoopNode* NodeA, const FPoint2D& ACoordinates, FIsoNode* NodeB, const FPoint2D& BCoordinates, const double FlatAngle)
{

#ifdef DEBUG_TRY_TO_CREATE_SEGMENT
	{
		F3DDebugSession _(TEXT("Test"));
		DisplaySegment(NodeA->Get2DPoint(EGridSpace::UniformScaled, Grid), NodeB->Get2DPoint(EGridSpace::UniformScaled, Grid), 0, EVisuProperty::RedCurve);
		//Wait(false);
	}
#endif

	if (NodeA->GetSegmentConnectedTo(NodeB))
	{
		return false;
	}

	if (InnerSegmentsIntersectionTool.DoesIntersect(*NodeA, *NodeB))
	{
		return false;
	}

	if (InnerToLoopSegmentsIntersectionTool.DoesIntersect(*NodeA, *NodeB))
	{
		return false;
	}

	if (Cell.IntersectionTool.DoesIntersect(*NodeA, *NodeB))
	{
		return false;
	}

	if (LoopSegmentsIntersectionTool.DoesIntersect(*NodeA, *NodeB))
	{
		return false;
	}

	// Is Outside and not too flat at NodeA
	if (NodeA->IsSegmentBeInsideFace(BCoordinates, Grid, FlatAngle))
	{
		return false;
	}

	// Is Outside and not too flat at NodeB
	if (NodeB->IsALoopNode())
	{
		if (((FLoopNode*)NodeB)->IsSegmentBeInsideFace(ACoordinates, Grid, FlatAngle))
		{
			return false;
		}
	}

	FIsoSegment& Segment = IsoSegmentFactory.New();
	Segment.Init(*NodeA, *NodeB, ESegmentType::LoopToLoop);
	Segment.SetCandidate();
	Cell.CandidateSegments.Add(&Segment);

#ifdef DEBUG_TRY_TO_CREATE_SEGMENT
	DisplaySegment(ACoordinates, BCoordinates, 0, EVisuProperty::OrangePoint);
#endif
	return true;
};

void FIsoTriangulator::ConnectCellCornerToInnerLoop(FCell& Cell)
{
	FIsoInnerNode* CellNodes[4];
	int32 Index = Cell.Id;
	CellNodes[0] = GlobalIndexToIsoInnerNodes[Index++];
	CellNodes[1] = GlobalIndexToIsoInnerNodes[Index];
	Index += Grid.GetCuttingCount(EIso::IsoU);;
	CellNodes[2] = GlobalIndexToIsoInnerNodes[Index--];
	CellNodes[3] = GlobalIndexToIsoInnerNodes[Index];

	{
		int32 ICell = 0;
		for (; ICell < 4; ++ICell)
		{
			if (CellNodes[ICell])
			{
				break;
			}
		}
		if (ICell == 4)
		{
			// All Cell corners are not null
			return;
		}
	}

#ifdef DEBUG_DELAUNAY
	F3DDebugSession _(TEXT("With cell corners"));
#endif

	TFunction<void(int32, FIsoInnerNode*)> FindAndTryCreateCandidateSegmentToLinkLoopToCorner = [&](int32 IndexLoopA, FIsoInnerNode* InnerNode)
	{

		const FPoint2D& InnerCoordinates = InnerNode->Get2DPoint(EGridSpace::UniformScaled, Grid);

		const TArray<FLoopNode*>& LoopA = Cell.SubLoops[IndexLoopA];

		double MinDistanceSquare = HUGE_VALUE_SQUARE;
		int32 MinIndexA = -1;
		for (int32 IndexA = 0; IndexA < LoopA.Num(); ++IndexA)
		{
			const FLoopNode* NodeA = LoopA[IndexA];
			const FPoint2D& ACoordinates = NodeA->Get2DPoint(EGridSpace::UniformScaled, Grid);

			double SquareDistance = ACoordinates.SquareDistance(InnerCoordinates);
			if (SquareDistance < MinDistanceSquare)
			{
				MinDistanceSquare = SquareDistance;
				MinIndexA = IndexA;
			}
		}

		if (MinIndexA >= 0)
		{
			FLoopNode* NodeA = LoopA[MinIndexA];
			const FPoint2D& ACoordinates = NodeA->Get2DPoint(EGridSpace::UniformScaled, Grid);

			TryToCreateSegment(Cell, NodeA, ACoordinates, InnerNode, InnerCoordinates, 0.1);
		}
	};

	int32 IntersectionToolCount = Cell.IntersectionTool.Count();
	int32 NewSegmentCount = Cell.CandidateSegments.Num() - IntersectionToolCount;
	Cell.IntersectionTool.AddSegments(Cell.CandidateSegments.GetData() + IntersectionToolCount, NewSegmentCount);
	Cell.IntersectionTool.Sort();

	for (int32 ICell = 0; ICell < 4; ++ICell)
	{
		if (CellNodes[ICell])
		{
			for (int32 IndexLoopA : Cell.BorderLoopIndices)
			{
				DisplayPoint(CellNodes[ICell]->Get2DPoint(EGridSpace::UniformScaled, Grid), EVisuProperty::GreenPoint);
				FindAndTryCreateCandidateSegmentToLinkLoopToCorner(IndexLoopA, CellNodes[ICell]);
			}

			if (Cell.bHasOuterLoop)
			{
				FindAndTryCreateCandidateSegmentToLinkLoopToCorner(0, CellNodes[ICell]);
			}
		}
	}

	Cell.SelectSegmentInCandidateSegments(IsoSegmentFactory);
}

} //namespace UE::CADKernel