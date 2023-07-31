// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CADKernel/Core/Chrono.h"
#include "CADKernel/Core/Types.h"
#include "CADKernel/Topo/TopologicalEdge.h"
#include "CADKernel/Topo/Shell.h"
#include "CADKernel/UI/Progress.h"

#ifdef CADKERNEL_DEV
#include "CADKernel/Mesh/Meshers/MesherReport.h"
#endif

namespace UE::CADKernel
{
class FCriterion;
class FGrid;
class FThinZoneSide;
class FTopologicalEntity;
class FTopologicalLoop;

struct FCostToFace
{
	double Cost;
	FTopologicalFace* Face;

	FCostToFace(double NewCost, FTopologicalFace* NewFace)
		: Cost(NewCost)
		, Face(NewFace)
	{
	}
};

class CADKERNEL_API FParametricMesher
{
protected:

	/**
	 * Limit of flatness of quad face
	 */
	const double ConstMinCurvature = 0.001;

	FModelMesh& MeshModel;

	TArray<FTopologicalFace*> Faces;

#ifdef CADKERNEL_DEV
	FMesherReport MesherReport;
	bool bDisplay = false;
#endif

public:

	FParametricMesher(FModelMesh& InMeshModel);

	const FModelMesh& GetMeshModel() const
	{
		return MeshModel;
	}

	FModelMesh& GetMeshModel()
	{
		return MeshModel;
	}

	void MeshEntities(TArray<FTopologicalShapeEntity*>& InEntities);

	void MeshEntity(FTopologicalShapeEntity& InEntity)
	{
		TArray<FTopologicalShapeEntity*> Entities;
		Entities.Add(&InEntity);
		MeshEntities(Entities);
	}

	void Mesh(FTopologicalFace& Face);
	void Mesh(FTopologicalEdge& InEdge, const FTopologicalFace& CarrierFace);
	void Mesh(FTopologicalVertex& Vertex);

	void MeshFaceLoops(FGrid& Grid);

	void MeshThinZoneEdges(FGrid&);
	void MeshThinZoneSide(const FThinZoneSide& Side);
	void GetThinZoneBoundary(const FThinZoneSide& Side);

	void GenerateCloud(FGrid& Grid);


#ifdef CADKERNEL_DEV
	void SetMeshReport(const FMesherReport& InMesherReport)
	{
		MesherReport = InMesherReport;
	}

	const FMesherReport& GetMeshReport() const 
	{
		return MesherReport;
	}
#endif

protected:

	void MeshEntities();


	void IsolateQuadFace(TArray<FCostToFace>& QuadSurfaces, TArray<FTopologicalFace*>& OtherSurfaces) const;

	void LinkQuadSurfaceForMesh(TArray<FCostToFace>& QuadTrimmedSurfaceSet, TArray<TArray<FTopologicalFace*>>& OutStrips);
	void MeshSurfaceByFront(TArray<FCostToFace>& QuadTrimmedSurfaceSet);

	void ApplyEdgeCriteria(FTopologicalEdge& Edge);
	void ApplyFaceCriteria(FTopologicalFace& Face);

	/**
	 * Generate Edge Elements on active edge from Edge cutting points
	 */
	void GenerateEdgeElements(FTopologicalEdge& Edge);
};

} // namespace UE::CADKernel

