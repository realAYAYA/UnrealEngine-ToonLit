// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CADKernel/Core/Entity.h"
#include "CADKernel/Core/EntityGeom.h"
#include "CADKernel/Core/Session.h"
#include "CADKernel/Core/Types.h"
#include "CADKernel/Mesh/Structure/Mesh.h"

namespace UE::CADKernel
{
class FCriterion;
class FDatabase;
class FEdgeMesh;
class FFaceMesh;
class FMesh;
class FPoint;
class FSession;
class FVertexMesh;

class CADKERNEL_API FModelMesh : public FEntityGeom
{
	friend FEntity;

	TArray<TSharedPtr<FCriterion>> Criteria;

	TArray<TArray<FPoint>*> GlobalPointCloud;
	FIdent LastIdUsed;

	TArray<TSharedPtr<FVertexMesh>> VertexMeshes;
	TArray<TSharedPtr<FEdgeMesh>> EdgeMeshes;
	TArray<TSharedPtr<FFaceMesh>> FaceMeshes;

	bool QuadAnalyse = false;
	double MinSize = DOUBLE_SMALL_NUMBER;
	double MaxSize = HUGE_VALUE;
	double MaxAngle = DOUBLE_PI;
	double Sag = HUGE_VALUE;

	FModelMesh()
		: FEntityGeom()
		, LastIdUsed(0)
	{
	}

public:

	int32 GetFaceCount() const
	{
		return FaceMeshes.Num();
	}

	int32 GetVertexCount() const
	{
		return LastIdUsed;
	}

	int32 GetTriangleCount() const;

	virtual void SpawnIdent(FDatabase& Database) override
	{
		if (!FEntity::SetId(Database))
		{
			return;
		}

		SpawnIdentOnEntities(VertexMeshes, Database);
		SpawnIdentOnEntities(EdgeMeshes, Database);
		SpawnIdentOnEntities(FaceMeshes, Database);
	};

	virtual void ResetMarkersRecursively()
	{
		ResetMarkers();
		ResetMarkersRecursivelyOnEntities(VertexMeshes);
		ResetMarkersRecursivelyOnEntities(EdgeMeshes);
		ResetMarkersRecursivelyOnEntities(FaceMeshes);
	};


	virtual EEntity GetEntityType() const override
	{
		return EEntity::MeshModel;
	}

	const TArray<TSharedPtr<FCriterion>>& GetCriteria() const
	{
		return Criteria;
	}

	void AddCriterion(TSharedPtr<FCriterion>& Criterion);

	double GetGeometricTolerance() const
	{
		return 0.02; //Session->GetGeometricTolerance();
	}

	double GetMinSize() const
	{
		return MinSize;
	}

	double GetMaxSize() const
	{
		return MaxSize;
	}

	double GetAngleCriteria()
	{
		return MaxAngle;
	}

	double GetSag() const
	{
		return Sag;
	}

	void AddMesh(TSharedRef<FVertexMesh> Mesh)
	{
		VertexMeshes.Add(Mesh);
	}

	void AddMesh(TSharedRef<FEdgeMesh> Mesh)
	{
		EdgeMeshes.Add(Mesh);
	}

	void AddMesh(TSharedRef<FFaceMesh> Mesh)
	{
		FaceMeshes.Add(Mesh);
	}

	void RegisterCoordinates(TArray<FPoint>& Coordinates, int32& OutStartVertexId, int32& OutIndex)
	{
		OutIndex = (int32)GlobalPointCloud.Num();
		OutStartVertexId = LastIdUsed;

		GlobalPointCloud.Add(&Coordinates);
		LastIdUsed += (int32)Coordinates.Num();
	}

	const int32 GetIndexOfVertexFromId(const int32 Id) const;
	const int32 GetIndexOfEdgeFromId(const int32 Id) const;
	const int32 GetIndexOfSurfaceFromId(const int32 Id) const;

	const TSharedPtr<FVertexMesh> GetMeshOfVertexNodeId(const int32 Id) const;

	void GetNodeCoordinates(TArray<FPoint>& NodeCoordinates) const;
	void GetNodeCoordinates(TArray<FVector3f>& NodeCoordinates) const;

	const TArray<TSharedPtr<FMesh>>& GetMeshes() const;

	const TArray<TSharedPtr<FFaceMesh>>& GetFaceMeshes() const
	{
		return FaceMeshes;
	}

#ifdef CADKERNEL_DEV
	virtual FInfoEntity& GetInfo(FInfoEntity&) const override;
#endif
};

}

