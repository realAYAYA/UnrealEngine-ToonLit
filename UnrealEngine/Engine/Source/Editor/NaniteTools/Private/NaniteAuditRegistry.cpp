// Copyright Epic Games, Inc. All Rights Reserved.

#include "NaniteAuditRegistry.h"
#include "NaniteSceneProxy.h"
#include "Engine/StaticMesh.h"
#include "Engine/InstancedStaticMesh.h"
//#include "Streaming/StreamingManagerTexture.h"

FNaniteAuditRegistry::FNaniteAuditRegistry()
{
}

void FNaniteAuditRegistry::PerformAudit()
{
	//FRenderAssetStreamingManager* Streamer = nullptr;
	/*
	if (!IStreamingManager::HasShutdown() && !!CVarSetTextureStreaming.GetValueOnAnyThread())
	{
		Streamer = (FRenderAssetStreamingManager*)&IStreamingManager::Get().GetRenderAssetStreamingManager();
		Streamer->UpdateResourceStreaming(0.f, true);
	}*/

	//Collect usage counts
	TMap<UStaticMesh*, TArray<UStaticMeshComponent*>> UsageList;
	TMap<UStaticMesh*, int32> InstanceCounts;

	bool bOnlyGameWorld = false;
	bool bUnknownRefOnly = false;

	for (TObjectIterator<UStaticMeshComponent> It; It; ++It)
	{
		UStaticMeshComponent* MeshComponent = *It;
		if (bOnlyGameWorld && (!MeshComponent->GetWorld() || !MeshComponent->GetWorld()->IsGameWorld()))
		{
			continue;
		}

		UStaticMesh* Mesh = MeshComponent->GetStaticMesh();

		TArray<UStaticMeshComponent*>& MeshUsageArray = UsageList.FindOrAdd(Mesh);
		MeshUsageArray.Add(MeshComponent);

		int32& InstanceCount = InstanceCounts.FindOrAdd(Mesh, 0);
		if (UInstancedStaticMeshComponent* ISM = Cast<UInstancedStaticMeshComponent>(MeshComponent))
		{
			InstanceCount += ISM->GetInstanceCount();
		}
		else
		{
			InstanceCount++;
		}
	}

	ErrorRecords.Empty();
	OptimizeRecords.Empty();

	for (TObjectIterator<UStaticMesh> It; It; ++It)
	{
		UStaticMesh* Mesh = *It;

		bool bUnknownRef = false;
		bool bIsStreaming = false;// (Mesh != nullptr ? Mesh->GetStreamingIndex() != INDEX_NONE : false);

		/*if (Streamer)
		{
			FStreamingRenderAsset* Asset = Streamer->GetStreamingRenderAsset(Mesh);
			if (Asset)
			{
				bUnknownRef = Asset->bUseUnkownRefHeuristic;
			}
		}*/

		if (bUnknownRefOnly && !bUnknownRef)
		{
			continue;
		}

		if (!Mesh->HasValidRenderData())
		{
			check(false);
			continue;
		}

		const TArray<UStaticMeshComponent*>* MeshUsageList = UsageList.Find(Mesh);
		int32 ComponentUsageCount = MeshUsageList ? MeshUsageList->Num() : 0;

		int32* InstanceCountPtr = InstanceCounts.Find(Mesh);
		int32 InstanceUsageCount = InstanceCountPtr ? *InstanceCountPtr : 0;

		if (bOnlyGameWorld && !InstanceUsageCount)
		{
			continue;
		}

		TSharedPtr<FNaniteAuditRecord> AuditRecord = MakeShared<FNaniteAuditRecord>();
		AuditRecord->StaticMesh = Mesh;
		AuditRecord->InstanceCount = InstanceUsageCount;
		AuditRecord->LODCount = Mesh->GetNumLODs();
		if (MeshUsageList)
		{
			for (UStaticMeshComponent* SMComponent : *MeshUsageList)
			{
				FStaticMeshComponentRecord& SMRecord = AuditRecord->StaticMeshComponents.AddDefaulted_GetRef();
				SMRecord.Component = SMComponent;
				Nanite::AuditMaterials(SMComponent, SMRecord.MaterialAudit);
			}
		}

		if (Mesh->NaniteSettings.bEnabled)
		{
			AuditRecord->TriangleCount = Mesh->GetNumNaniteTriangles();
			ErrorRecords.Emplace(AuditRecord);
		}
		else
		{
			// Grab the non-Nanite triangle count from LOD0
			AuditRecord->TriangleCount = Mesh->GetNumTriangles(0);
			OptimizeRecords.Emplace(AuditRecord);
		}
	}

	auto SortPredicate = [](const TSharedPtr<FNaniteAuditRecord>& A, const TSharedPtr<FNaniteAuditRecord>& B)
	{
		if (A->TriangleCount > B->TriangleCount)
			return true;
		else if (A->TriangleCount < B->TriangleCount)
			return false;

		if (A->InstanceCount > B->InstanceCount)
			return true;
		else if (A->InstanceCount < B->InstanceCount)
			return false;

		return (A->LODCount > B->LODCount);
	};

	ErrorRecords.Sort(SortPredicate);
	OptimizeRecords.Sort(SortPredicate);
}