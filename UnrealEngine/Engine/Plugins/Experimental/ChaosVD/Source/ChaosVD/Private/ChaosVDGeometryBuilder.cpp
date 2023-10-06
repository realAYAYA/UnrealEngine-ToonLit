// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosVDGeometryBuilder.h"

#include "DynamicMeshToMeshDescription.h"
#include "MeshConstraints.h"
#include "MeshConstraintsUtil.h"
#include "MeshSimplification.h"
#include "StaticMeshAttributes.h"
#include "UDynamicMesh.h"
#include "UObject/UObjectGlobals.h"

static FAutoConsoleVariable CVarChaosVDGeometryToProcessPerTick(
	TEXT("p.Chaos.VD.Tool.GeometryToProcessPerTick"),
	50,
	TEXT("Number of generated geometry to process each tick when loading a teace file in the CVD tool"));

void FChaosVDGeometryBuilder::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddStableReferenceMap(DynamicMeshCacheMap);
	Collector.AddStableReferenceMap(StaticMeshCacheMap);
}

bool FChaosVDGeometryBuilder::DoesImplicitContainType(const Chaos::FImplicitObject* InImplicitObject, const Chaos::EImplicitObjectType ImplicitTypeToCheck)
{
	using namespace Chaos;
	const EImplicitObjectType InnerType = GetInnerType(InImplicitObject->GetType());

	switch (InnerType)
	{
		case ImplicitObjectType::Union:
			{
				const FImplicitObjectUnion* Union = InImplicitObject->template GetObject<FImplicitObjectUnion>();

				for (int i = 0; i < Union->GetObjects().Num(); ++i)
				{
					const TUniquePtr<FImplicitObject>& UnionImplicit = Union->GetObjects()[i];

					if (DoesImplicitContainType(UnionImplicit.Get(), ImplicitTypeToCheck))
					{
						return true;
					}
				}

				return false;
				break;
			}
		case ImplicitObjectType::Transformed:
			{
				const TImplicitObjectTransformed<FReal, 3>* Transformed = InImplicitObject->template GetObject<TImplicitObjectTransformed<FReal, 3>>();
				return DoesImplicitContainType(Transformed->GetTransformedObject(), ImplicitTypeToCheck);
				break;
			}
	default:
		return InnerType == ImplicitTypeToCheck;
	}

	return false;
}

bool FChaosVDGeometryBuilder::HasNegativeScale(const Chaos::FRigidTransform3& InTransform) const
{
	FVector ScaleSignVector = InTransform.GetScale3D().GetSignVector();
	return ScaleSignVector.X * ScaleSignVector.Y * ScaleSignVector.Z < 0;
}

UDynamicMesh* FChaosVDGeometryBuilder::CreateAndCacheDynamicMesh(const uint32 GeometryCacheKey, UE::Geometry::FMeshShapeGenerator& MeshGenerator)
{
	{
		FReadScopeLock ReadLock(RWLock);
		//TODO: Make this return what is cached when the system is more robust
		// For now this should not happen and we want to catch it and make it visually noticeable
		if (DynamicMeshCacheMap.Contains(GeometryCacheKey))
		{
			ensureMsgf(false, TEXT("Tried to create a new mesh with an existing Cache key"));
			return nullptr;
		}
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(FChaosVDGeometryBuilder::CreateAndCacheDynamicMesh_BUILD);

	UDynamicMesh* Mesh = NewObject<UDynamicMesh>();
	Mesh->SetMesh(&MeshGenerator.Generate());

	{
		FWriteScopeLock WriteLock(RWLock);
		DynamicMeshCacheMap.Add(GeometryCacheKey, Mesh);
	}

	return Mesh;
}

UStaticMesh* FChaosVDGeometryBuilder::CreateAndCacheStaticMesh(const uint32 GeometryCacheKey, UE::Geometry::FMeshShapeGenerator& MeshGenerator, const int32 LODsToGenerateNum)
{
	{
		FReadScopeLock ReadLock(RWLock);
		//TODO: Make this return what is cached when the system is more robust
		// For now this should not happen and we want to catch it and make it visually noticeable
		if (StaticMeshCacheMap.Contains(GeometryCacheKey))
		{
			ensureMsgf(false, TEXT("Tried to create a new mesh with an existing Cache key"));
			return nullptr;
		}
	}

	//TODO: Instead of generating a dynamic mesh and discard it, we should
	// Create a Mesh description directly when no LODs are required.
	// We could create a base class for our mesh Generators and add a Generate method that generates these mesh descriptions
	UStaticMesh* MainStaticMesh = NewObject<UStaticMesh>();
	MainStaticMesh->GetStaticMaterials().Add(FStaticMaterial());

	const int32 MeshDescriptionsToGenerate = LODsToGenerateNum + 1;

	TArray<const FMeshDescription*> LODDescriptions;
	LODDescriptions.Reserve(MeshDescriptionsToGenerate);

	MainStaticMesh->SetNumSourceModels(MeshDescriptionsToGenerate);

	FDynamicMesh3 DynamicMesh(&MeshGenerator.Generate());

	for (int32 i = 0; i < MeshDescriptionsToGenerate; i++)
	{
		if (i > 0)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FChaosVDGeometryBuilder::CreateAndCacheStaticMesh_LOD);
			//TODO: Come up with a better algo for this.
			const int32 DesiredTriangleCount = DynamicMesh.TriangleCount() / (i * 2);
			// Simplify
			UE::Geometry::FMeshConstraints Constraints;
			UE::Geometry::FMeshConstraintsUtil::ConstrainAllBoundariesAndSeams(Constraints, DynamicMesh,
																			   UE::Geometry::EEdgeRefineFlags::NoFlip, UE::Geometry::EEdgeRefineFlags::NoConstraint, UE::Geometry::EEdgeRefineFlags::NoConstraint,
																			   false, false, true);
			// Reduce the same previous LOD Mesh on each iteration
			UE::Geometry::FQEMSimplification Simplifier(&DynamicMesh);
			Simplifier.SetExternalConstraints(MoveTemp(Constraints));
			Simplifier.SimplifyToTriangleCount(DesiredTriangleCount);
		}

		FMeshDescription* MeshDescription = new FMeshDescription();
		FStaticMeshAttributes Attributes(*MeshDescription);
		Attributes.Register();

		FDynamicMeshToMeshDescription Converter;
		Converter.Convert(&DynamicMesh, *MeshDescription, true);
		LODDescriptions.Add(MeshDescription);
	}

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FChaosVDGeometryBuilder::CreateAndCacheStaticMesh_BUILD);
		UStaticMesh::FBuildMeshDescriptionsParams Params;
		Params.bUseHashAsGuid = true;
		Params.bMarkPackageDirty = false;
		Params.bBuildSimpleCollision = false;
		Params.bCommitMeshDescription = false;
		Params.bFastBuild = true;

		MainStaticMesh->NaniteSettings.bEnabled = true;
		MainStaticMesh->BuildFromMeshDescriptions(LODDescriptions, Params);

		MainStaticMesh->bAutoComputeLODScreenSize = true;
	}

	{
		FWriteScopeLock WriteLock(RWLock);
		StaticMeshCacheMap.Add(GeometryCacheKey, MainStaticMesh);
	}

	for (const FMeshDescription* Desc : LODDescriptions)
	{
		delete Desc;
		Desc = nullptr;
	}

	LODDescriptions.Reset();

	return MainStaticMesh;
}

void FChaosVDGeometryBuilder::ApplyMeshToComponentFromKey(TWeakObjectPtr<UMeshComponent> MeshComponent, const uint32 GeometryKey)
{
	if (!MeshComponent.IsValid())
	{
		return;
	}

	if (UDynamicMeshComponent* DynamicMeshComponent = Cast<UDynamicMeshComponent>(MeshComponent))
	{
		if (TObjectPtr<UDynamicMesh>* DynamicMesh = DynamicMeshCacheMap.Find(GeometryKey))
		{
			DynamicMeshComponent->SetDynamicMesh(*DynamicMesh);
		}
	}
	else if (UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(MeshComponent))
	{
		if (TObjectPtr<UStaticMesh>* StaticMesh = StaticMeshCacheMap.Find(GeometryKey))
		{
			StaticMeshComponent->SetStaticMesh(*StaticMesh);
		}
	}
}

bool FChaosVDGeometryBuilder::GameThreadTick(float DeltaTime)
{
	int32 CurrentGeometryProcessed = 0;
	while (!GeometryReadyToApplyQueue.IsEmpty() && CurrentGeometryProcessed < CVarChaosVDGeometryToProcessPerTick->GetInt())
	{
		uint32 GeometryKey = 0;
		GeometryReadyToApplyQueue.Dequeue(GeometryKey);
		CurrentGeometryProcessed++;

		TArray<TWeakObjectPtr<UMeshComponent>>* MeshComponentsWaiting = nullptr;
		{
			FReadScopeLock ReadLock(RWLock);
			 MeshComponentsWaiting = MeshComponentsWaitingForGeometryByKey.Find(GeometryKey);
		}

		if (MeshComponentsWaiting)
		{
			for (const TWeakObjectPtr<UMeshComponent>& MeshComponent : *MeshComponentsWaiting)
			{
				ApplyMeshToComponentFromKey(MeshComponent, GeometryKey);
			}
		}
		
		{
			FWriteScopeLock WriteLock(RWLock);
			MeshComponentsWaitingForGeometryByKey.Remove(GeometryKey);
		}
	}

	return true;
}

void FChaosVDGeometryBuilder::RegisterMeshComponentWaitingForGeometry(uint32 GeometryKey, TWeakObjectPtr<UMeshComponent> MesComponent, const int32 LODsToGenerateNum)
{
	if (!MesComponent.IsValid())
	{
		return;
	}

	const bool bIsMeshTypeLODCompatible = !MesComponent->IsA(UInstancedStaticMeshComponent::StaticClass()) && !MesComponent->IsA(UDynamicMeshComponent::StaticClass());
	if (LODsToGenerateNum > 0 && !bIsMeshTypeLODCompatible)
	{
		ensureMsgf(false, TEXT("LODs are currently not supported with the specified mesh component [%s] in the CVD tool | [%d] LODs were requested when 0 is expected"), *MesComponent->GetName(), LODsToGenerateNum);
	}

	{
		FWriteScopeLock WriteLock(RWLock);
	
		if (TArray<TWeakObjectPtr<UMeshComponent>>* MeshComponentsWaiting = MeshComponentsWaitingForGeometryByKey.Find(GeometryKey))
		{
			MeshComponentsWaiting->Add(MesComponent);
		}
		else
		{
			MeshComponentsWaitingForGeometryByKey.Add(GeometryKey, {MesComponent});
		}
	}
}
