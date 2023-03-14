// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CADKernel/Core/Factory.h"
#include "CADKernel/Math/Geometry.h"
#include "CADKernel/Math/Point.h"
#include "CADKernel/Math/SlopeUtils.h"
#include "CADKernel/Mesh/Meshers/IsoTriangulator/IntersectionSegmentTool.h"
#include "CADKernel/Mesh/Meshers/IsoTriangulator/IsoNode.h"
#include "CADKernel/Mesh/Meshers/IsoTriangulator/IsoSegment.h"
#include "CADKernel/UI/Visu.h"

#ifdef CADKERNEL_DEV
#include "CADKernel/Mesh/Meshers/IsoTriangulator/DefineForDebug.h"
#include "CADKernel/Mesh/Meshers/MesherReport.h"
#endif

//#define NEED_TO_CHECK_USEFULNESS
//#define DEBUG_DELAUNAY
namespace UE::CADKernel
{

class FGrid;
class FIntersectionSegmentTool;
class FFaceMesh;
struct FCell;


struct CADKERNEL_API FIsoTriangulatorChronos
{
	FDuration TriangulateDuration1 = FChrono::Init();
	FDuration TriangulateDuration2 = FChrono::Init();
	FDuration TriangulateDuration3 = FChrono::Init();
	FDuration TriangulateDuration4 = FChrono::Init();
	FDuration TriangulateDuration = FChrono::Init();
	FDuration BuildIsoNodesDuration = FChrono::Init();
	FDuration BuildLoopSegmentsDuration = FChrono::Init();
	FDuration BuildInnerSegmentsDuration = FChrono::Init();
	FDuration FindLoopSegmentOfInnerTriangulationDuration = FChrono::Init();
	FDuration FindSegmentIsoUVSurroundingSmallLoopDuration = FChrono::Init();
	FDuration FindIsoSegmentToLinkInnerToLoopDuration = FChrono::Init();
	FDuration FindInnerSegmentToLinkLoopToLoopDuration = FChrono::Init();
	FDuration FindSegmentToLinkLoopToLoopDuration = FChrono::Init();
	FDuration FindSegmentToLinkLoopToLoopByDelaunayDuration = FChrono::Init();
	FDuration FindSegmentToLinkInnerToLoopDuration = FChrono::Init();
	FDuration TriangulateOverCycleDuration = FChrono::Init();
	FDuration TriangulateInnerNodesDuration = FChrono::Init();

	FIsoTriangulatorChronos()
	{}

	void PrintTimeElapse() const
	{
		FDuration IsoTriangulerDuration = FChrono::Init();
		IsoTriangulerDuration += BuildIsoNodesDuration;
		IsoTriangulerDuration += BuildLoopSegmentsDuration;
		IsoTriangulerDuration += BuildInnerSegmentsDuration;
		IsoTriangulerDuration += FindLoopSegmentOfInnerTriangulationDuration;
		IsoTriangulerDuration += FindIsoSegmentToLinkInnerToLoopDuration;
		IsoTriangulerDuration += FindSegmentToLinkLoopToLoopDuration;
		IsoTriangulerDuration += FindSegmentToLinkLoopToLoopByDelaunayDuration;
		IsoTriangulerDuration += FindSegmentToLinkInnerToLoopDuration;
		IsoTriangulerDuration += TriangulateOverCycleDuration;
		IsoTriangulerDuration += TriangulateInnerNodesDuration;

		FChrono::PrintClockElapse(Log, TEXT(""), TEXT("IsoTrianguler"), IsoTriangulerDuration);
		FChrono::PrintClockElapse(Log, TEXT("  "), TEXT("Triangulate"), TriangulateDuration);
		FChrono::PrintClockElapse(Log, TEXT("    "), TEXT("BuildIsoNodes"), BuildIsoNodesDuration);
		FChrono::PrintClockElapse(Log, TEXT("    "), TEXT("BuildLoopSegments"), BuildLoopSegmentsDuration);
		FChrono::PrintClockElapse(Log, TEXT("    "), TEXT("BuildInnerSegments"), BuildInnerSegmentsDuration);
		FChrono::PrintClockElapse(Log, TEXT("    "), TEXT("FindLoopSegmentOfInnerTriangulation"), FindLoopSegmentOfInnerTriangulationDuration);
		FChrono::PrintClockElapse(Log, TEXT("      "), TEXT("FindSegmentIsoUVSurroundingSmallLoop"), FindSegmentIsoUVSurroundingSmallLoopDuration);
		FChrono::PrintClockElapse(Log, TEXT("    "), TEXT("Find IsoSegment ToLink InnerToLoop"), FindIsoSegmentToLinkInnerToLoopDuration);
		FChrono::PrintClockElapse(Log, TEXT("    "), TEXT("Find Segment ToLink LoopToLoop by Delaunay"), FindSegmentToLinkLoopToLoopByDelaunayDuration);
		FChrono::PrintClockElapse(Log, TEXT("    "), TEXT("Find Segment ToLink LoopToLoop"), FindSegmentToLinkLoopToLoopDuration);
		FChrono::PrintClockElapse(Log, TEXT("    "), TEXT("Find Segment ToLink InnerToLoop"), FindSegmentToLinkInnerToLoopDuration);
		FChrono::PrintClockElapse(Log, TEXT("    "), TEXT("Mesh Over Cycle"), TriangulateOverCycleDuration);
		FChrono::PrintClockElapse(Log, TEXT("    "), TEXT("Mesh Inner Nodes"), TriangulateInnerNodesDuration);
		FChrono::PrintClockElapse(Log, TEXT("  "), TEXT("Triangulate1"), TriangulateDuration1);
		FChrono::PrintClockElapse(Log, TEXT("  "), TEXT("Triangulate2"), TriangulateDuration2);
		FChrono::PrintClockElapse(Log, TEXT("  "), TEXT("Triangulate3"), TriangulateDuration3);
		FChrono::PrintClockElapse(Log, TEXT("  "), TEXT("Triangulate4"), TriangulateDuration4);
		FChrono::PrintClockElapse(Log, TEXT("  "), TEXT("Triangulate "), TriangulateDuration);
	}
};

class FIsoTriangulator
{
	friend class FParametricMesher;
	friend class FLoopCleaner;

protected:

	FGrid& Grid;
	TSharedRef<FFaceMesh> Mesh;

	TArray<int32> LoopStartIndex;
	TArray<FLoopNode> LoopNodes;
	int32 LoopNodeCount = 0;
	TArray<FLoopNode*> SortedLoopNodes;

	/**
	 * GlobalIndexToIsoInnerNodes contains only inner nodes of the grid, if GlobalIndexToIsoInnerNodes[Index] == null, the point is outside the domain
	 */
	TArray<FIsoInnerNode*> GlobalIndexToIsoInnerNodes;

	/**
	 * Static array of InnerNodes. Only used for allocation needs
	 */
	TArray<FIsoInnerNode> InnerNodes;
	int32 InnerNodeCount = 0;



	TFactory<FIsoSegment> IsoSegmentFactory;

	TArray<FIsoSegment*> LoopSegments;
	TArray<FIsoSegment*> ThinZoneSegments;
	TArray<FIsoSegment*> FinalInnerSegments;
	TArray<FIsoSegment*> InnerToOuterSegments;

	/**
	 * Tool to check if a segment intersect or not existing segments.
	 * To be optimal, depending on the segment, only a subset of segment is used.
	 * Checks intersection with loop.
	 */
	FIntersectionSegmentTool LoopSegmentsIntersectionTool;
	FIntersectionSegmentTool InnerSegmentsIntersectionTool;
	FIntersectionSegmentTool InnerToLoopSegmentsIntersectionTool;
	FIntersectionSegmentTool InnerToOuterSegmentsIntersectionTool;


	/**
	 * Define all the lower left index of grid node that the upper cell is surrounding a loop
	 * This is set in FindSegmentIsoUVSurroundingSmallLoop
	 * and use in TriangulateInnerNodes to don't generate the both cell triangles
	 */
	TArray<int32> IndexOfLowerLeftInnerNodeSurroundingALoop;

	/**
	 * Segments to link inner to boundary and boundary to boundary
	 * From FindIsoSegmentToLinkInnerToBoundary
	 * SelectSegmentInCandidateSegments
	 */
	TArray<FIsoSegment*> FinalToLoops;

	/**
	 *
	 */
	TArray<FIsoSegment*> CandidateSegments;

	TArray<FIsoSegment*> NewTestSegments;

	bool bDisplay = false;

	bool bNeedCheckOrientation = false;

	static const double GeometricToMeshingToleranceFactor;
	const double GeometricTolerance;
	const double SquareGeometricTolerance;
	const double SquareGeometricTolerance2;
	const double MeshingTolerance;
	const double SquareMeshingTolerance;

#ifdef CADKERNEL_DEV
	FMesherReport* MesherReport;
#endif

public:

	FIsoTriangulator(FGrid& InGrid, TSharedRef<FFaceMesh> EntityMesh);

#ifdef CADKERNEL_DEV
	void SetMesherReport(FMesherReport& InMesherReport)
	{
		MesherReport = &InMesherReport;
	}
#endif

	/**
	 * Main method
	 * @return false if the tessellation failed
	 */
	bool Triangulate();

	void BuildNodes();

	/**
	 * Build the segments of the loops and check if each loop is self intersecting.
	 * @return false if the loop is self intersecting
	 */
	void BuildLoopSegments();

	void BuildInnerSegments();

	/**
	 * Add temporary loops defining thin zones to avoid the tessellation of these zone.
	 * These zones are tessellated in a specific process
	 */
	void BuildThinZoneSegments();

	/**
	 * Fill mesh node data (Position, normal, UV, Index) of the FFaceMesh object
	 */
	void FillMeshNodes();

	/**
	 * Build the Inner Segments Intersection Tool used to check if a candidate segment crosses the inner mesh
	 *
	 * The minimal set of segments of the intersection tool is the boundaries of the inner triangulation.
	 *
	 * https://docs.google.com/presentation/d/1qUVOH-2kU_QXBVKyRUcdDy1Y6WGkcaJCiaS8wGjSZ6M/edit?usp=sharing
	 * Slide "Boundary Segments Of Inner Triangulation"
	 */
	void BuildInnerSegmentsIntersectionTool();

	/**
	 * The purpose of the method is to add surrounding segments (boundary of an unitary inner grid cell) to the small loop to intersection tool to prevent traversing inner segments
	 * A loop is inside inner segments
	 *									|			 |
	 *								   -----------------
	 *									|	 XXX	 |
	 *									|	XXXXX	 |
	 *									|	 XXX	 |
	 *								   -----------------
	 *									|			 |
	 * https://docs.google.com/presentation/d/1qUVOH-2kU_QXBVKyRUcdDy1Y6WGkcaJCiaS8wGjSZ6M/edit?usp=sharing
	 * Slide "Find Inner Grid Cell Surrounding Small Loop"
	 * This method finalizes BuildInnerSegmentsIntersectionTool
	 */
	void FindInnerGridCellSurroundingSmallLoop();

#ifdef NEED_TO_CHECK_USEFULNESS
	/**
	 *
	 */
	void CompleteIsoSegmentLoopToLoop();
#endif

	void ConnectCellLoops();
	void FindCellContainingBoundaryNodes(TArray<FCell>& Cells);

	/**
	 * The closest loops are connected together
	 * To do it, a Delaunay triangulation of the loop barycenter is realized.
	 * Each edge of this mesh defined a near loops pair
	 * The shortest segment is then build between this pair of loops
	 */
	void ConnectCellSubLoopsByNeighborhood(FCell& cell);

	void FindIsoSegmentToLinkOuterLoopNodes(FCell& Cell);	

	void FindSegmentToLinkOuterLoopNodes(FCell& Cell);

	void FindSegmentToLinkOuterToInnerLoopNodes(FCell& Cell);

	void ConnectCellCornerToInnerLoop(FCell& Cell);

	/**
	 * The goal of this algorithm is to connect iso U (or V) aligned loop nodes as soon as they are nearly in the same iso V (or U) strip.
	 * I.e.:
	 * - Iso U aligned: NodeA.U = NodeB.U +/-TolU
	 * - In the same strip: each node of the segment has the same index "i" that verify: isoV[i] - TolV < Node.V < isoV[i+1] + TolV
	 */
	void FindIsoSegmentToLinkLoopToLoop();

	/**
	 * The purpose of the method is select a minimal set of segments connecting loops together
	 * The final segments will be selected with SelectSegmentInCandidateSegments
	 */
	void ConnectCellSubLoopsByNeighborhood();  // to rename and clean

	void FindCandidateSegmentsToLinkInnerAndLoop();

	/**
	 * Complete the final set of segments with the best subset of segments in CandidateSegments
	 */
	void SelectSegmentInCandidateSegments();

	/**
	 * The goal of this algorithm is to connect unconnected inner segment extremity i.e. extremity with one or less connected segment to the closed boundary node
	 */
	void ConnectUnconnectedInnerSegments();

	/**
	 * Finalize the tessellation between inner grid boundary and loops.
	 * The final set of segments define a network
	 * Each minimal cycle is tessellated independently
	 */
	void TriangulateOverCycle(const EGridSpace Space);

	/**
	 * Find in the network a minimal cycle stating from a segment
	 * @return false if the new cycle crosses a segement already used
	 */
	bool FindCycle(FIsoSegment* StartSegment, bool bLeftSide, TArray<FIsoSegment*>& Cycle, TArray<bool>& CycleOrientation);

	/**
	 * Generate the "Delaunay" tessellation of the cycle.
	 * The algorithm is based on frontal process
	 */
	void MeshCycle(const EGridSpace Space, const TArray<FIsoSegment*>& cycle, const TArray<bool>& cycleOrientation);

#ifdef WIP_ADD_STEP_TO_TO_FAVOR_ISO_SEGMENTS
	void FindIsoCandidateSegmentInCycle(TArray<FIsoNode*> CycleNodes);
#endif

	bool CanCycleBeMeshed(const TArray<FIsoSegment*>& Cycle, FIntersectionSegmentTool& CycleIntersectionTool);

	/**
	 * Finalization of the mesh by the tessellation of the inner grid
	 */
	void TriangulateInnerNodes();

	/**
	 * Sorted loop node array used to have efficient loop proximity node research
	 */
	void SortLoopNodes()
	{
		SortedLoopNodes.Reserve(LoopNodes.Num());
		for (FLoopNode& LoopNode : LoopNodes)
		{
			SortedLoopNodes.Add(&LoopNode);
		}

		Algo::Sort(SortedLoopNodes, [this](const FLoopNode* Node1, const FLoopNode* Node2)
			{
				const FPoint2D& Node1Coordinates = Node1->Get2DPoint(EGridSpace::Default2D, Grid);
				const FPoint2D& Node2Coordinates = Node2->Get2DPoint(EGridSpace::Default2D, Grid);
				return (Node1Coordinates.U + Node1Coordinates.V) < (Node2Coordinates.U + Node2Coordinates.V);
			});
	}

private:

	FIsoSegment* FindNextSegment(EGridSpace Space, const FIsoSegment* StartSegment, const FIsoNode* StartNode, SlopMethod GetSlop) const;

	// ==========================================================================================
	// 	   Create segments
	// ==========================================================================================

	/**
	 *  SubLoopA                  SubLoopB
	 *      --X---X             X-----X--
	 *             \           /
	 *              \         /
	 *               X=======X
	 *              /         \
	 *             /           \
	 *      --X---X             X-----X--
	 *
	 *     ======= ShortestSegment
	 */
	void TryToConnectTwoSubLoopsWithShortestSegment(FCell& Cell, const TArray<FLoopNode*>& SubLoopA, const TArray<FLoopNode*>& SubLoopB);

	void TryToConnectTwoLoopsWithIsocelesTriangle(FCell& Cell, const TArray<FLoopNode*>& SubLoopA, const TArray<FLoopNode*>& SubLoopB);

	/**
	 *    --X------X-----X--  <-- SubLoopA
	 *             I
	 *             I    <- The most isoSegment
	 *             I
	 *    ----X----X--------  <-- SubLoopB
	 *
	 */
	void TryToConnectTwoSubLoopsWithTheMostIsoSegment(FCell& Cell, const TArray<FLoopNode*>& SubLoopA, const TArray<FLoopNode*>& SubLoopB);

	/**
	 *    X--X------X-----X--  <-- SubLoop
	 *    |         I
	 *    |         I   <- The most isoSegment
	 *    |         I
	 *    X----X----X--------
	 *
	 */
	void TryToConnectVertexSubLoopWithTheMostIsoSegment(FCell& Cell, const TArray<FLoopNode*>& SubLoop);
	bool TryToCreateSegment(FCell& Cell, FLoopNode* NodeA, const FPoint2D& ACoordinates, FIsoNode* NodeB, const FPoint2D& BCoordinates, const double FlatAngle);

#ifdef CADKERNEL_DEV
public:
	FIsoTriangulatorChronos Chronos;

	void DisplayPixels(TArray<uint8>& Pixel) const;

	void DisplayPixel(const int32 Index) const;
	void DisplayPixel(const int32 IndexU, const int32 IndexV) const;

	void DisplayIsoNodes(EGridSpace Space) const;

	void DisplayLoops(const TCHAR* Message, bool bOneNode = true, bool bSplitBySegment = false) const;
	void DisplayLoopsByNextAndPrevious(const TCHAR* Message) const;

	void DisplayCells(const TArray<FCell>& Cells) const;
	void DisplayCell(const FCell& Cell) const;
	void DrawCellBoundary(int32 Index, EVisuProperty Property) const;
#endif

};

namespace IsoTriangulatorImpl
{

/**
 * Criteria to find the optimal "Delaunay" triangle starting from the segment AB to a set of point P
 * A "Delaunay" triangle is an equilateral triangle
 * The optimal value is the smallest value.
 */
inline double CotangentCriteria(const FPoint& APoint, const FPoint& BPoint, const FPoint& PPoint, FPoint& OutNormal)
{
	const double BigValue = HUGE_VALUE;

	FPoint PA = APoint - PPoint;
	FPoint PB = BPoint - PPoint;

	// the ratio between the scalar product PA.PB (=|PA| |PB| cos (A,P,B) )
	// with the norm of the cross product |PA^PB| (=|PA| |PB| |sin(A,P,B)|)
	// is compute. 
	double ScalareProduct = PA * PB;
	OutNormal = PA ^ PB;
	double NormOFScalarProduct = sqrt(OutNormal * OutNormal);

	// PPoint is aligned with (A,B)
	if (NormOFScalarProduct < DOUBLE_SMALL_NUMBER)
	{
		return BigValue;
	}

	// return Cotangent value 
	return ScalareProduct / NormOFScalarProduct;
}

inline double CotangentCriteria(const FPoint2D& APoint, const FPoint2D& BPoint, const FPoint2D& PPoint)
{
	const double BigValue = HUGE_VALUE;

	FPoint2D PA = APoint - PPoint;
	FPoint2D PB = BPoint - PPoint;

	// the ratio between the scalar product PA.PB (=|PA| |PB| cos (A,P,B) )
	// with the norm of the cross product |PA^PB| (=|PA| |PB| |sin(A,P,B)|)
	// is compute. 
	double ScalareProduct = PA * PB;
	double OutNormal = PA ^ PB;
	double NormOFPointProduct = FMath::Abs(OutNormal);

	if (NormOFPointProduct < DOUBLE_SMALL_NUMBER)
	{
		// PPoint is aligned with (A,B)
		return BigValue;
	}

	// return Cotangent value 
	return ScalareProduct / NormOFPointProduct;
}

template<class PointType>
inline double IsoscelesCriteria(const PointType& APoint, const PointType& BPoint, const PointType& IsoscelesVertex)
{
	double Coord = CoordinateOfProjectedPointOnSegment(IsoscelesVertex, APoint, BPoint, false);
	return FMath::Abs(Coord - 0.5);
}

template<class PointType>
inline double EquilateralCriteria(const PointType& SegmentA, const PointType& SegmentB, const PointType& Point)
{
	double Criteria1 = FMath::Abs(CoordinateOfProjectedPointOnSegment(SegmentA, SegmentB, Point, false) - 0.5);
	double Criteria2 = FMath::Abs(CoordinateOfProjectedPointOnSegment(Point, SegmentA, SegmentB, false) - 0.5);
	double Criteria3 = FMath::Abs(CoordinateOfProjectedPointOnSegment(SegmentB, Point, SegmentA, false) - 0.5);
	return Criteria1 + Criteria2 + Criteria3;
}


} // namespace FIsoTriangulatorImpl

} // namespace UE::CADKernel

