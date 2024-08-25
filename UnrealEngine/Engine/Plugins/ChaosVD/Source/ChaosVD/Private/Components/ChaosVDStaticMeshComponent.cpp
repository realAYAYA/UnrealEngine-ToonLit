// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/ChaosVDStaticMeshComponent.h"

#include "ChaosVDModule.h"
#include "MaterialDomain.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"

uint32 UChaosVDStaticMeshComponent::GetGeometryKey() const
{
	return CurrentGeometryKey;
}

void UChaosVDStaticMeshComponent::UpdateInstanceVisibility(const TSharedPtr<FChaosVDMeshDataInstanceHandle>& InInstanceHandle, bool bIsVisible)
{
	if (!InInstanceHandle.IsValid())
	{
		UE_LOG(LogChaosVDEditor, Warning, TEXT("[%s] Attempted to update a mesh instance using an invalid handle. No instances were updated"), ANSI_TO_TCHAR(__FUNCTION__));
		return;
	}

	if (InInstanceHandle->GetMeshComponent() != this)
	{
		UE_LOG(LogChaosVDEditor, Error, TEXT("[%s] Attempted to update a mesh instance using a handle from another component. No instances were updated | Handle Component [%s] | Current Component [%s]"), ANSI_TO_TCHAR(__FUNCTION__), *GetNameSafe(InInstanceHandle->GetMeshComponent()), *GetNameSafe(this));
		return;
	}

	SetVisibility(bIsVisible);
}

void UChaosVDStaticMeshComponent::SetIsSelected(const TSharedPtr<FChaosVDMeshDataInstanceHandle>& InInstanceHandle, bool bIsSelected)
{
	bIsOwningParticleSelected = bIsSelected;

	PushSelectionToProxy();
}

bool UChaosVDStaticMeshComponent::ShouldRenderSelected() const
{
	return bIsOwningParticleSelected;
}

void UChaosVDStaticMeshComponent::UpdateInstanceColor(const TSharedPtr<FChaosVDMeshDataInstanceHandle>& InInstanceHandle, FLinearColor NewColor)
{
	if (!InInstanceHandle.IsValid())
	{
		UE_LOG(LogChaosVDEditor, Warning, TEXT("[%s] Attempted to update a mesh instance using an invalid handle. No instances were updated"), ANSI_TO_TCHAR(__FUNCTION__));
		return;
	}

	if (InInstanceHandle->GetMeshComponent() != this)
	{
		UE_LOG(LogChaosVDEditor, Error, TEXT("[%s] Attempted to update a mesh instance using a handle from another component. No instances were updated | Handle Component [%s] | Current Component [%s]"), ANSI_TO_TCHAR(__FUNCTION__), *GetNameSafe(InInstanceHandle->GetMeshComponent()), *GetNameSafe(this));
		return;
	}

	const bool bIsSolidColor = FMath::IsNearlyEqual(NewColor.A, 1.0f);

	if (UMaterialInstanceDynamic* MaterialToApply = GetCachedMaterialInstance(bIsSolidColor ? EChaosVDMaterialType::SimOnlyMaterial : EChaosVDMaterialType::QueryOnlyMaterial))
	{
		MaterialToApply->SetVectorParameterValue(TEXT("BaseColor"), NewColor);
		InInstanceHandle->GetMeshComponent()->SetMaterial(0, MaterialToApply);
	}
	else
	{
		ensure(false);
		UE_LOG(LogChaosVDEditor, Error, TEXT("[%s] Failed to get Query Only material for, applying the default mesh to all geometry"), ANSI_TO_TCHAR(__FUNCTION__));
			
		InInstanceHandle->GetMeshComponent()->SetMaterial(0, UMaterial::GetDefaultMaterial(EMaterialDomain::MD_Surface));
	}
}

void UChaosVDStaticMeshComponent::UpdateInstanceWorldTransform(const TSharedPtr<FChaosVDMeshDataInstanceHandle>& InInstanceHandle, const FTransform& InTransform)
{
	if (!InInstanceHandle.IsValid())
	{
		UE_LOG(LogChaosVDEditor, Warning, TEXT("[%s] Attempted to update a mesh instance using an invalid handle. No instances were updated"), ANSI_TO_TCHAR(__FUNCTION__));
		return;
	}

	if (InInstanceHandle->GetMeshComponent() != this)
	{
		UE_LOG(LogChaosVDEditor, Error, TEXT("[%s] Attempted to update a mesh instance using a handle from another component. No instances were updated | Handle Component [%s] | Current Component [%s]"), ANSI_TO_TCHAR(__FUNCTION__), *GetNameSafe(InInstanceHandle->GetMeshComponent()), *GetNameSafe(this));
		return;
	}

	SetWorldTransform(InTransform);
}

void UChaosVDStaticMeshComponent::Reset()
{
	bIsMeshReady = false;
	MeshReadyDelegate = FChaosVDMeshReadyDelegate();
	ComponentEmptyDelegate = FChaosVDMeshComponentEmptyDelegate();

	CurrentMeshDataHandle = nullptr;
	CurrentGeometryKey = 0;
}

TSharedPtr<FChaosVDMeshDataInstanceHandle> UChaosVDStaticMeshComponent::AddMeshInstance(const FTransform InstanceTransform, bool bIsWorldSpace, const TSharedPtr<FChaosVDExtractedGeometryDataHandle>& InGeometryHandle, int32 ParticleID, int32 SolverID)
{
	// Static meshes only have one instance
	constexpr int32 StaticMeshInstanceIndex = 0;

	CurrentMeshDataHandle = MakeShared<FChaosVDMeshDataInstanceHandle>(StaticMeshInstanceIndex, this, ParticleID, SolverID);
	CurrentMeshDataHandle->SetGeometryHandle(InGeometryHandle);

	const uint32 NewHandleGeometryKey = InGeometryHandle->GetGeometryKey();

	if (!UpdateGeometryKey(NewHandleGeometryKey))
	{
		return nullptr;
	}

	CurrentMeshDataHandle->SetWorldTransform(InstanceTransform);

	return CurrentMeshDataHandle;
}

void UChaosVDStaticMeshComponent::AddMeshInstanceForHandle(TSharedPtr<FChaosVDMeshDataInstanceHandle> MeshDataHandle, const FTransform InstanceTransform, bool bIsWorldSpace, const TSharedPtr<FChaosVDExtractedGeometryDataHandle>& InGeometryHandle, int32 ParticleID, int32 SolverID)
{
	// Static meshes only have one instance
	constexpr int32 StaticMeshInstanceIndex = 0;

	const uint32 NewHandleGeometryKey = InGeometryHandle->GetGeometryKey();
	if (!UpdateGeometryKey(NewHandleGeometryKey))
	{
		return;
	}

	MeshDataHandle->SetMeshInstanceIndex(StaticMeshInstanceIndex);
	MeshDataHandle->SetMeshComponent(this);
	
	CurrentMeshDataHandle->SetWorldTransform(InstanceTransform);
}

void UChaosVDStaticMeshComponent::RemoveMeshInstance(TSharedPtr<FChaosVDMeshDataInstanceHandle> InHandleToRemove)
{
	SetStaticMesh(nullptr);
	CurrentMeshDataHandle = nullptr;
	ComponentEmptyDelegate.Broadcast(this);
}

bool UChaosVDStaticMeshComponent::UpdateGeometryKey(uint32 NewHandleGeometryKey)
{
	if (CurrentGeometryKey != 0 && CurrentGeometryKey != NewHandleGeometryKey)
	{
		ensure(false);

		UE_LOG(LogChaosVDEditor, Warning, TEXT("[%s] Attempted to add a mesh instance belonging to another geometry key. No instance was added | CurrentKey [%u] | New Key [%u]"), ANSI_TO_TCHAR(__FUNCTION__), CurrentGeometryKey, NewHandleGeometryKey);
		return false;
	}
	else
	{
		CurrentGeometryKey = NewHandleGeometryKey;
	}

	return true;
}


TSharedPtr<FChaosVDMeshDataInstanceHandle> UChaosVDStaticMeshComponent::GetMeshDataInstanceHandle(int32 InstanceIndex) const
{
	return CurrentMeshDataHandle;
}

TArrayView<TSharedPtr<FChaosVDMeshDataInstanceHandle>> UChaosVDStaticMeshComponent::GetMeshDataInstanceHandles()
{
	return TArrayView<TSharedPtr<FChaosVDMeshDataInstanceHandle>>(&CurrentMeshDataHandle, 1);
}

UMaterialInstanceDynamic* UChaosVDStaticMeshComponent::GetCachedMaterialInstance(EChaosVDMaterialType Type)
{
	if (const TObjectPtr<UMaterialInstanceDynamic>* MaterialInstance = CachedMaterialInstancesByID.Find(Type))
	{
		return MaterialInstance->Get();
	}
	else
	{
		if (UMaterialInterface* MaterialToCreate = FChaosVDGeometryComponentUtils::GetBaseMaterialForType(Type))
		{
			UMaterialInstanceDynamic* DynamicMaterial = UMaterialInstanceDynamic::Create(MaterialToCreate, nullptr);
			CachedMaterialInstancesByID.Add(Type, TObjectPtr<UMaterialInstanceDynamic>(DynamicMaterial));

			return DynamicMaterial;
		}
	}

	return nullptr;
}
