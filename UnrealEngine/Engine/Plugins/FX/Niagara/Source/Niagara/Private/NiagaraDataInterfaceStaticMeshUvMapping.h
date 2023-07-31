// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Engine/StaticMesh.h"
#include "Experimental/NiagaraMeshUvMapping.h"
#include "NiagaraUvQuadTree.h"
#include "RenderResource.h"
#include "UObject/WeakObjectPtrTemplates.h"

class UStaticMesh;
struct FStaticMeshLODResources;

struct FStaticMeshUvMapping : public FMeshUvMapping
{
	FStaticMeshUvMapping(TWeakObjectPtr<UStaticMesh> InMeshObject, int32 InLodIndex, int32 InUvSetIndex);
	virtual ~FStaticMeshUvMapping() = default;

	static bool IsValidMeshObject(const TWeakObjectPtr<UStaticMesh>& MeshObject, int32 InLodIndex, int32 InUvSetIndex);

	bool Matches(const TWeakObjectPtr<UStaticMesh>& InMeshObject, int32 InLodIndex, int32 InUvSetIndex) const;

	const FStaticMeshLODResources* GetLodRenderData() const;

	void FindOverlappingTriangles(const FVector2D& InUv, float Tolerance, TArray<int32>& TriangleIndices) const;
	int32 FindFirstTriangle(const FVector2D& InUv, float Tolerance, FVector& BarycentricCoord) const;
	int32 FindFirstTriangle(const FBox2D& InUvBox, FVector& BarycentricCoord) const;

private:
	static const FStaticMeshLODResources* GetLodRenderData(const UStaticMesh& Mesh, int32 LodIndex, int32 UvSetIndex);
	virtual void BuildQuadTree() override;

	TWeakObjectPtr<UStaticMesh> MeshObject;
};