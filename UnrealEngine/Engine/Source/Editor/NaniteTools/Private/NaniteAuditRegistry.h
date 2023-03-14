// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/StaticMesh.h"
#include "NaniteSceneProxy.h"

class UMaterial;
class UMaterialInterface;
class UStaticMesh;
class UInstancedStaticMesh;

struct FNaniteMaterialError
{
	TWeakObjectPtr<const UMaterial> Material;
	TArray<TWeakObjectPtr<UStaticMeshComponent>> ReferencingSMCs;
	uint8 bUnsupportedBlendModeError	: 1;
	uint8 bWorldPositionOffsetError : 1;
	uint8 bVertexInterpolatorError : 1;
	uint8 bPixelDepthOffsetError : 1;
};

struct FStaticMeshComponentRecord
{
	TWeakObjectPtr<UStaticMeshComponent> Component;
	Nanite::FMaterialAudit MaterialAudit;
};

struct FNaniteAuditRecord : TSharedFromThis<FNaniteAuditRecord>
{
	TWeakObjectPtr<UStaticMesh> StaticMesh = nullptr;
	TArray<FStaticMeshComponentRecord> StaticMeshComponents;
	TArray<FNaniteMaterialError> MaterialErrors;
	uint32 ErrorCount = 0;
	uint32 InstanceCount = 0;
	uint32 TriangleCount = 0;
	uint32 LODCount = 0;
};

class FNaniteAuditRegistry
{
public:
	FNaniteAuditRegistry();

	void PerformAudit();

	inline TArray<TSharedPtr<FNaniteAuditRecord>>& GetErrorRecords()
	{
		return ErrorRecords;
	}

	inline TArray<TSharedPtr<FNaniteAuditRecord>>& GetOptimizeRecords()
	{
		return OptimizeRecords;
	}

private:
	TArray<TSharedPtr<FNaniteAuditRecord>> ErrorRecords;
	TArray<TSharedPtr<FNaniteAuditRecord>> OptimizeRecords;
};