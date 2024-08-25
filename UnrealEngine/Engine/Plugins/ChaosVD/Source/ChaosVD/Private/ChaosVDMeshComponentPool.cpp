// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosVDMeshComponentPool.h"

#include "ChaosVDGeometryDataComponent.h"
#include "ChaosVDModule.h"
#include "UObject/Package.h"

bool FChaosVDMeshComponentPool::bUseComponentsPool = true;
FAutoConsoleVariableRef FChaosVDMeshComponentPool::CVarUseComponentsPool(TEXT("p.Chaos.VD.Tool.UseComponentsPool"), FChaosVDMeshComponentPool::bUseComponentsPool, TEXT("Set to false to disable the use of a pool system for Mesh Components."));

void FChaosVDMeshComponentPool::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObjects(PooledDynamicMeshComponent);
	Collector.AddReferencedObjects(PooledInstancedStaticMeshComponent);
	Collector.AddReferencedObjects(PooledStaticMeshComponent);
}

void FChaosVDMeshComponentPool::DisposeMeshComponent(UMeshComponent* MeshComponent)
{
	if (!MeshComponent)
	{
		return;
	}

	if (!bUseComponentsPool)
	{
		MeshComponent->DestroyComponent();
		return;
	}

	ResetMeshComponent(MeshComponent);

	if (UInstancedStaticMeshComponent* AsISMC = Cast<UInstancedStaticMeshComponent>(MeshComponent))
	{
		PooledInstancedStaticMeshComponent.Add(AsISMC);
	}
	else if (UStaticMeshComponent* AsSMC = Cast<UStaticMeshComponent>(MeshComponent))
	{
		PooledStaticMeshComponent.Add(AsSMC);
	}
	else if (UDynamicMeshComponent* AsDMC = Cast<UDynamicMeshComponent>(MeshComponent))
	{
		PooledDynamicMeshComponent.Add(AsDMC);
	}
	else
	{
		UE_LOG(LogChaosVDEditor, Warning, TEXT("[%s] Unable top dispose Mesh component [%s] | Incompatible mesh type used [%s]"), ANSI_TO_TCHAR(__FUNCTION__), *GetNameSafe(MeshComponent), *GetNameSafe(MeshComponent->GetClass()));
	}

	const FString NewName = MeshComponent->GetName() + TEXT("_POOLED_") + FGuid::NewGuid().ToString();
	MeshComponent->Rename(*NewName, GetTransientPackage(), REN_NonTransactional | REN_DoNotDirty | REN_ForceNoResetLoaders | REN_SkipGeneratedClasses | REN_DontCreateRedirectors);
}

void FChaosVDMeshComponentPool::ResetMeshComponent(UMeshComponent* MeshComponent)
{
	if (!MeshComponent)
	{
		return;
	}

	if (UDynamicMeshComponent* DynamicMeshComponent = Cast<UDynamicMeshComponent>(MeshComponent))
	{
		DynamicMeshComponent->SetDynamicMesh(nullptr);
	}
	else if (UInstancedStaticMeshComponent* InstancedStaticMeshComponent = Cast<UInstancedStaticMeshComponent>(MeshComponent))
	{
		InstancedStaticMeshComponent->SetStaticMesh(nullptr);
		InstancedStaticMeshComponent->ClearInstances();
	}
	else if (UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(MeshComponent))
	{
		StaticMeshComponent->SetStaticMesh(nullptr);
	}
	
	MeshComponent->SetRelativeTransform(FTransform::Identity);
	
	if (IChaosVDGeometryComponent* DataComponent = Cast<IChaosVDGeometryComponent>(MeshComponent))
	{
		DataComponent->Reset();
	}

	MeshComponent->UnregisterComponent();

	if (AActor* Owner = MeshComponent->GetOwner())
	{
		Owner->RemoveOwnedComponent(MeshComponent);
	}
}
