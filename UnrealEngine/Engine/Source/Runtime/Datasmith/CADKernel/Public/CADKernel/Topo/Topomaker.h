// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CADKernel/Core/Types.h"

#ifdef CADKERNEL_DEV
#include "CADKernel/Topo/TopomakerReport.h"
#endif

namespace UE::CADKernel
{

class FModel;
class FSession;
class FShell;
class FTopologicalEdge;
class FTopologicalFace;
class FTopologicalVertex;

class CADKERNEL_API FTopomaker
{

protected:

	FSession& Session;

	TArray<FShell*> Shells;
	TArray<TSharedPtr<FTopologicalFace>> Faces;

#ifdef CADKERNEL_DEV
	FTopomakerReport Report;
#endif

public:

	FTopomaker(FSession& InSession, double InTolerance, double InForceFactor);
	FTopomaker(FSession& InSession, const TArray<TSharedPtr<FShell>>& Shells, double InTolerance, double InForceFactor);
	FTopomaker(FSession& InSession, const TArray<TSharedPtr<FTopologicalFace>>& Surfaces, double InTolerance, double InForceFactor);

	void SetTolerance(double InTolerance, double InForceFactor)
	{
		Tolerance = InTolerance;
		ForceJoinFactor = InForceFactor;

		SewTolerance = Tolerance * UE_DOUBLE_SQRT_2;

		// StartVertex (SV) is linked to TwinStartVertex (TSV), the max distance between vertices is SewTolerance
		// EndVertex (EV) is linked to TwinEndVertex (TEV), the max distance between vertices is SewTolerance
		//
		//           * TSV * TEV
		//     SV *-----------* EV
		// So the max length is 2*SewTolerance
		EdgeLengthTolerance = SewTolerance * 2.;

		SewToleranceToForceJoin = Tolerance * ForceJoinFactor;
		LargeEdgeLengthTolerance = SewToleranceToForceJoin * 2.;

		ThinFaceWidth = Tolerance * ForceJoinFactor;
	}

	void Sew(bool bForceJoining, bool bRemoveThinFaces);

	/**
	 * Check topology of each body
	 */
	void CheckTopology();

	/**
	 * Split into connected shell and put each shell into the appropriate body
	 */
	void SplitIntoConnectedShells();

	void OrientShells();

	/**
	 * Unlink Non-Manifold Vertex i.e. Vertex belong to tow or more shell
	 */
	void UnlinkNonManifoldVertex();

	void RemoveThinFaces(TArray<FTopologicalEdge*>& NewBorderEdges);

#ifdef CADKERNEL_DEV
	void PrintSewReport()
	{
		Report.PrintSewReport();
	}

	void PrintRemoveThinFacesReport()
	{
		Report.PrintRemoveThinFacesReport();
	}
#endif

private:

	/**
	 * Call by constructor.
	 * For each shell, add their faces into Faces array, complete the metadata and set the states for the joining process
	 */
	void InitFaces();

	void EmptyShells();

	void RemoveFacesFromShell();

	void RemoveEmptyShells();

	/**
	 * Return an array of active vertices.
	 */
	void GetVertices(TArray<TSharedPtr<FTopologicalVertex>>& Vertices);

	/**
	 * Return an array of active border vertices.
	 */
	void GetBorderVertices(TArray<TSharedPtr<FTopologicalVertex>>& BorderVertices);

	void CheckSelfConnectedEdge(double MaxLengthOfDegeneratedEdge, TArray<TSharedPtr<FTopologicalVertex>>& OutBorderVertices);
	void RemoveIsolatedEdges(); // Useful ???

	void SetSelfConnectedEdgeDegenerated(TArray<TSharedPtr<FTopologicalVertex>>& Vertices);

	/**
	 * For each loop of each surface, check if successive edges are unconnected and if their common vertices are connected only to them (UV vs CV: vertex connected to many faces ).
	 * These edges must be tangent together.
	 * These edges are merged into one edge.
	 * E.g. :
	 * Face A has 3 successive unconnected edges. If these 3 edges are merged to give only one edge, the new edge could be linked to its parallel edge of Face B
	 * 
	 *  CV: Connected vertex i.e. linked to different faces
	 * 
	 *
	 *              \                          Face A                                   |    Face C
	 *               \                                                                  |
	 *    Face E     CV ------------------ UV -------------------- UV ---------------- CV --------------------
	 *               CV -------------------------------------------------------------- CV --------------------
	 *               /                         Face B                                   |    Face D
	 *              /                                                                   |
	 */
	void MergeUnconnectedSuccessiveEdges();

	double Tolerance;
	double ForceJoinFactor;

	double SewTolerance;
	double SewToleranceToForceJoin; // SewTolerance x ForceJoinFactor;

	double EdgeLengthTolerance; // 2x SewTolerance
	double LargeEdgeLengthTolerance; // 2x SewToleranceToForceJoin

	double ThinFaceWidth;
};

} // namespace UE::CADKernel
