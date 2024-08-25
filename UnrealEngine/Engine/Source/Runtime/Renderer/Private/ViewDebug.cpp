// Copyright Epic Games, Inc. All Rights Reserved.

#include "ViewDebug.h"

#if !UE_BUILD_SHIPPING

#include "ScenePrivate.h"
#include "Materials/Material.h"
#include "MaterialShared.h"
#include "ProfilingDebugging/DiagnosticTable.h"
#include "MeshPassProcessor.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/MaterialInterface.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "StaticMeshBatch.h"

static bool bDumpPrimitivesNextFrame = false;
static bool bDumpDetailedPrimitivesNextFrame = false;

static FAutoConsoleCommand CVarDumpPrimitives(
	TEXT("DumpPrimitives"),
	TEXT("Writes out all scene primitive names to a CSV file"),
	FConsoleCommandDelegate::CreateStatic([] { bDumpPrimitivesNextFrame = true; }),
	ECVF_Default);

static FAutoConsoleCommand CVarDrawPrimitiveDebugData(
	TEXT("DumpDetailedPrimitives"),
	TEXT("Writes out all scene primitive details to a CSV file"),
	FConsoleCommandDelegate::CreateStatic([] { bDumpDetailedPrimitivesNextFrame = !bDumpDetailedPrimitivesNextFrame; }),
	ECVF_Default);

static uint32 GetDrawCountFromPrimitiveSceneInfo(FScene* Scene, const FPrimitiveSceneInfo* PrimitiveSceneInfo)
{
	uint32 DrawCount = 0;
	for (const FCachedMeshDrawCommandInfo& CachedCommand : PrimitiveSceneInfo->StaticMeshCommandInfos)
	{
		if (CachedCommand.MeshPass != EMeshPass::BasePass)
			continue;

		if (CachedCommand.StateBucketId != INDEX_NONE || CachedCommand.CommandIndex >= 0)
		{
			DrawCount++;
		}
	}

	return DrawCount;
}

FViewDebugInfo FViewDebugInfo::Instance;

FViewDebugInfo::FViewDebugInfo()
{
	bHasEverUpdated = false;
	bIsOutdated = true;
	bShouldUpdate = false;
	bShouldCaptureSingleFrame = false;
}

void FViewDebugInfo::ProcessPrimitive(FPrimitiveSceneInfo* PrimitiveSceneInfo, const FViewInfo& View, FScene* Scene, const IPrimitiveComponent* DebugComponentInterface)
{
	if (!DebugComponentInterface->IsRegistered())
	{
		return;
	}
	UObject* Actor = DebugComponentInterface->GetOwner();
	FString FullName = DebugComponentInterface->GetName();
	const uint32 DrawCount = GetDrawCountFromPrimitiveSceneInfo(Scene, PrimitiveSceneInfo);

	TArray<UMaterialInterface*> Materials;
	DebugComponentInterface->GetUsedMaterials(Materials);
	const int32 LOD = PrimitiveSceneInfo->Proxy ? PrimitiveSceneInfo->Proxy->GetLOD(&View) : INDEX_NONE;
	int32 Triangles = 0;
	
	const UObject* DebugComponent = DebugComponentInterface->GetUObject();

	FPrimitiveStats Stat(LOD);
	DebugComponentInterface->GetPrimitiveStats(Stat);

	const FPrimitiveInfo PrimitiveInfo = {
		Actor,
		PrimitiveSceneInfo->PrimitiveComponentId,
		const_cast<IPrimitiveComponent*>(DebugComponentInterface), // This is probably a bad idea, find alternative
		PrimitiveSceneInfo,
		MoveTemp(Materials),
		MoveTemp(FullName),
		DrawCount,
		Triangles,
		LOD
	};

	Primitives.Add(PrimitiveInfo);
}

void FViewDebugInfo::DumpToCSV() const
{
	const FString OutputPath = FPaths::ProfilingDir() / TEXT("Primitives") / FString::Printf(TEXT("PrimitivesDetailed-%s.csv"), *FDateTime::Now().ToString());
	const bool bSuppressViewer = true;
	FDiagnosticTableViewer DrawViewer(*OutputPath, bSuppressViewer);
	DrawViewer.AddColumn(TEXT("Name"));
	DrawViewer.AddColumn(TEXT("ActorClass"));
	DrawViewer.AddColumn(TEXT("Actor"));
	DrawViewer.AddColumn(TEXT("Location"));
	DrawViewer.AddColumn(TEXT("NumMaterials"));
	DrawViewer.AddColumn(TEXT("Materials"));
	DrawViewer.AddColumn(TEXT("NumDraws"));
	DrawViewer.AddColumn(TEXT("LOD"));
	DrawViewer.AddColumn(TEXT("Triangles"));
	DrawViewer.CycleRow();

	FRWScopeLock ScopeLock(Lock, SLT_ReadOnly);
	const FPrimitiveSceneInfo* LastPrimitiveSceneInfo = nullptr;
	for (const FPrimitiveInfo& Primitive : Primitives)
	{
		if (Primitive.PrimitiveSceneInfo != LastPrimitiveSceneInfo)
		{
			DrawViewer.AddColumn(*Primitive.Name);
			DrawViewer.AddColumn(Primitive.Owner ? *Primitive.Owner->GetClass()->GetName() : TEXT(""));
			DrawViewer.AddColumn(Primitive.Owner ? *Primitive.Owner->GetFullName() : TEXT(""));

			DrawViewer.AddColumn(Primitive.ComponentInterface ?
				*FString::Printf(TEXT("{%s}"), *Primitive.ComponentInterface->GetTransform().GetLocation().ToString()) : TEXT(""));
			DrawViewer.AddColumn(*FString::Printf(TEXT("%d"), Primitive.Materials.Num()));
			FString Materials = "[";
			for (int i = 0; i < Primitive.Materials.Num(); i++)
			{
				if (Primitive.Materials[i] && Primitive.Materials[i]->GetMaterial())
				{
					Materials += Primitive.Materials[i]->GetMaterial()->GetName();
				}
				else
				{
					Materials += "Null";
				}

				if (i < Primitive.Materials.Num() - 1)
				{
					Materials += ", ";
				}
			}
			Materials += "]";
			DrawViewer.AddColumn(*FString::Printf(TEXT("%s"), *Materials));
			DrawViewer.AddColumn(*FString::Printf(TEXT("%d"), Primitive.DrawCount));
			DrawViewer.AddColumn(*FString::Printf(TEXT("%d"), Primitive.LOD));
			DrawViewer.AddColumn(*FString::Printf(TEXT("%d"), Primitive.TriangleCount));
			DrawViewer.CycleRow();

			LastPrimitiveSceneInfo = Primitive.PrimitiveSceneInfo;
		}
	}
}

void FViewDebugInfo::CaptureNextFrame()
{
	FRWScopeLock ScopeLock(Lock, SLT_Write);
	bShouldCaptureSingleFrame = true;
	bShouldUpdate = true;
}

void FViewDebugInfo::EnableLiveCapture()
{
	FRWScopeLock ScopeLock(Lock, SLT_Write);
	bShouldCaptureSingleFrame = false;
	bShouldUpdate = true;
}

void FViewDebugInfo::DisableLiveCapture()
{
	FRWScopeLock ScopeLock(Lock, SLT_Write);
	bShouldCaptureSingleFrame = false;
	bShouldUpdate = false;
}

bool FViewDebugInfo::HasEverUpdated() const
{
	FRWScopeLock ScopeLock(Lock, SLT_ReadOnly);
	return bHasEverUpdated;
}

bool FViewDebugInfo::IsOutOfDate() const
{
	FRWScopeLock ScopeLock(Lock, SLT_ReadOnly);
	return bIsOutdated;
}

void FViewDebugInfo::ProcessPrimitives(FScene* Scene, const FViewInfo& View, const FViewCommands& ViewCommands)
{
	DumpPrimitives(Scene, ViewCommands);

	{
		FRWScopeLock ScopeLock(Lock, SLT_Write);
		bIsOutdated = true;

		if (!bShouldUpdate && !bDumpDetailedPrimitivesNextFrame)
		{
			return;
		}

		if (bShouldCaptureSingleFrame)
		{
			bShouldCaptureSingleFrame = false;
			bShouldUpdate = false;
		}

		// TODO: Add profiling to this function

		Primitives.Empty(ViewCommands.MeshCommands[EMeshPass::BasePass].Num() + ViewCommands.DynamicMeshCommandBuildRequests[EMeshPass::BasePass].Num());

		for (const FVisibleMeshDrawCommand& Mesh : ViewCommands.MeshCommands[EMeshPass::BasePass])
		{
			const int32 PrimitiveId = Mesh.PrimitiveIdInfo.ScenePrimitiveId;
			if (PrimitiveId >= 0 && PrimitiveId < Scene->Primitives.Num())
			{
				FPrimitiveSceneInfo* PrimitiveSceneInfo = Scene->Primitives[PrimitiveId];
				ProcessPrimitive(PrimitiveSceneInfo, View, Scene, PrimitiveSceneInfo->GetComponentInterfaceForDebugOnly());
			}
		}

		for (const FStaticMeshBatch* StaticMeshBatch : ViewCommands.DynamicMeshCommandBuildRequests[EMeshPass::BasePass])
		{
			FPrimitiveSceneInfo* PrimitiveSceneInfo = StaticMeshBatch->PrimitiveSceneInfo;
			ProcessPrimitive(PrimitiveSceneInfo, View, Scene, PrimitiveSceneInfo->GetComponentInterfaceForDebugOnly());
		}

		bHasEverUpdated = true;
		bIsOutdated = false;
	}
	OnUpdate.Broadcast();

	if (bDumpDetailedPrimitivesNextFrame)
	{
		DumpToCSV();
		bDumpDetailedPrimitivesNextFrame = false;
	}
}

void FViewDebugInfo::DumpPrimitives(FScene* Scene, const FViewCommands& ViewCommands)
{
	if (!bDumpPrimitivesNextFrame)
	{
		return;
	}

	bDumpPrimitivesNextFrame = false;

	struct FPrimitiveInfo
	{
		const FPrimitiveSceneInfo* PrimitiveSceneInfo;
		FString Name;
		uint32 DrawCount;

		bool operator<(const FPrimitiveInfo& Other) const
		{
			// Sort by name to group similar assets together, then by exact primitives so we can ignore duplicates
			const int32 NameCompare = Name.Compare(Other.Name);
			if (NameCompare != 0)
			{
				return NameCompare < 0;
			}

			return PrimitiveSceneInfo < Other.PrimitiveSceneInfo;
		}
	};

	TArray<FPrimitiveInfo> Primitives;
	Primitives.Reserve(ViewCommands.MeshCommands[EMeshPass::BasePass].Num() + ViewCommands.DynamicMeshCommandBuildRequests[EMeshPass::BasePass].Num());

	{
		for (const FVisibleMeshDrawCommand& Mesh : ViewCommands.MeshCommands[EMeshPass::BasePass])
		{
			int32 PrimitiveId = Mesh.PrimitiveIdInfo.ScenePrimitiveId;
			if (PrimitiveId >= 0 && PrimitiveId < Scene->Primitives.Num())
			{
				const FPrimitiveSceneInfo* PrimitiveSceneInfo = Scene->Primitives[PrimitiveId];
				FString FullName = PrimitiveSceneInfo->GetComponentForDebugOnly()->GetFullName();

				uint32 DrawCount = GetDrawCountFromPrimitiveSceneInfo(Scene, PrimitiveSceneInfo);

				Primitives.Add({ PrimitiveSceneInfo, MoveTemp(FullName), DrawCount });
			}
		}

		for (const FStaticMeshBatch* StaticMeshBatch : ViewCommands.DynamicMeshCommandBuildRequests[EMeshPass::BasePass])
		{
			const FPrimitiveSceneInfo* PrimitiveSceneInfo = StaticMeshBatch->PrimitiveSceneInfo;
			FString FullName = PrimitiveSceneInfo->GetComponentForDebugOnly()->GetFullName();

			uint32 DrawCount = GetDrawCountFromPrimitiveSceneInfo(Scene, PrimitiveSceneInfo);

			Primitives.Add({ PrimitiveSceneInfo, MoveTemp(FullName), DrawCount });
		}
	}

	Primitives.Sort();

	const FString OutputPath = FPaths::ProfilingDir() / TEXT("Primitives") / FString::Printf(TEXT("Primitives-%s.csv"), *FDateTime::Now().ToString());
	const bool bSuppressViewer = true;
	FDiagnosticTableViewer DrawViewer(*OutputPath, bSuppressViewer);
	DrawViewer.AddColumn(TEXT("Name"));
	DrawViewer.AddColumn(TEXT("NumDraws"));
	DrawViewer.CycleRow();

	const FPrimitiveSceneInfo* LastPrimitiveSceneInfo = nullptr;
	for (const FPrimitiveInfo& Primitive : Primitives)
	{
		if (Primitive.PrimitiveSceneInfo != LastPrimitiveSceneInfo)
		{
			DrawViewer.AddColumn(*Primitive.Name);
			DrawViewer.AddColumn(*FString::Printf(TEXT("%d"), Primitive.DrawCount));
			DrawViewer.CycleRow();

			LastPrimitiveSceneInfo = Primitive.PrimitiveSceneInfo;
		}
	}
}
#endif