// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/ChaosVDInstancedStaticMeshComponent.h"

#include "ChaosVDModule.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"

void UChaosVDInstancedStaticMeshComponent::UpdateInstanceVisibility(const TSharedPtr<FChaosVDMeshDataInstanceHandle>& InInstanceHandle, bool bIsVisible)
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

	const int32 InstanceIndex = InInstanceHandle->GetMeshInstanceIndex();

	FTransform Transform = InInstanceHandle->GetWorldTransform();
	if (!bIsVisible)
	{
		// Setting the scale to 0 will hide this instance while keeping it on the component
		Transform.SetScale3D(FVector::ZeroVector);
	}

	constexpr bool bIsWorldSpaceTransform = true;
	constexpr bool bMarkRenderDirty = true;
	constexpr bool bTeleport = true;

	UpdateInstanceTransform(InstanceIndex, Transform, bIsWorldSpaceTransform, bMarkRenderDirty, bTeleport);
}

void UChaosVDInstancedStaticMeshComponent::SetIsSelected(const TSharedPtr<FChaosVDMeshDataInstanceHandle>& InInstanceHandle, bool bIsSelected)
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

	const int32 InstanceIndex = InInstanceHandle->GetMeshInstanceIndex();

	if (!ensure(IsValidInstance(InstanceIndex)))
	{
		UE_LOG(LogChaosVDEditor, Error, TEXT("[%s] Attempted to update a mesh instance using a handle with an invalid instance index | Handle Instance Index [%d] | Current Instance Conut [%d]"), ANSI_TO_TCHAR(__FUNCTION__), InstanceIndex, GetInstanceCount());
		return;
	}

	NotifySMInstanceSelectionChanged({this, InstanceIndex}, bIsSelected);
}

void UChaosVDInstancedStaticMeshComponent::UpdateInstanceColor(const TSharedPtr<FChaosVDMeshDataInstanceHandle>& InInstanceHandle, FLinearColor NewColor)
{
	if (CurrentMaterial == nullptr)
	{
		CurrentMaterial = GetMaterial(0);
	}

	// Check that this mesh component supports the intended visualization
	// We can't change the material of Instanced mesh components because we might have other instances that are not intended to be translucent (or the other way around).
	// The Mesh handle instance system should have detected we need to migrate the instance to another component before ever reaching this point
	const bool bIsSolidColor = FMath::IsNearlyEqual(NewColor.A, 1.0f);
	const bool bMeshComponentSupportsTranslucentInstances = EnumHasAnyFlags(static_cast<EChaosVDMeshAttributesFlags>(MeshComponentAttributeFlags), EChaosVDMeshAttributesFlags::TranslucentGeometry);
	if (bIsSolidColor)
	{
		if (!ensure(!bMeshComponentSupportsTranslucentInstances))
		{
			UE_LOG(LogChaosVDEditor, Warning, TEXT("[%s] Desired Color is not supported in this mesh component [%s]..."), ANSI_TO_TCHAR(__FUNCTION__), *NewColor.ToString());
		}
	}
	else
	{
		if (!ensure(bMeshComponentSupportsTranslucentInstances))
		{
			UE_LOG(LogChaosVDEditor, Warning, TEXT("[%s] Desired Color is not supported in this mesh component [%s]..."), ANSI_TO_TCHAR(__FUNCTION__), *NewColor.ToString());
		}
	}
	
	SetNumCustomDataFloats(4);
	SetCustomData(InInstanceHandle->GetMeshInstanceIndex(), TArrayView<float>(reinterpret_cast<float*>(&NewColor), 4));
}

void UChaosVDInstancedStaticMeshComponent::UpdateInstanceWorldTransform(const TSharedPtr<FChaosVDMeshDataInstanceHandle>& InInstanceHandle, const FTransform& InTransform)
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

	const int32 InstanceIndex = InInstanceHandle->GetMeshInstanceIndex();

	FTransform NewTransform = InTransform;

	if (!InInstanceHandle->GetVisibility())
	{
		NewTransform.SetScale3D(FVector::ZeroVector);
	}

	constexpr bool bIsWorldSpaceTransform = true;
	constexpr bool bMarkRenderDirty = true;
	constexpr bool bTeleport = true;

	UpdateInstanceTransform(InstanceIndex, NewTransform, bIsWorldSpaceTransform, bMarkRenderDirty, bTeleport);
}

TArrayView<TSharedPtr<FChaosVDMeshDataInstanceHandle>> UChaosVDInstancedStaticMeshComponent::GetMeshDataInstanceHandles()
{
	return CurrentInstanceHandles;
}

void UChaosVDInstancedStaticMeshComponent::Reset()
{
	bIsMeshReady = false;
	MeshReadyDelegate = FChaosVDMeshReadyDelegate();
	ComponentEmptyDelegate = FChaosVDMeshComponentEmptyDelegate();
	CurrentMaterial = nullptr;

	CurrentInstanceHandles.Reset();
	CurrentGeometryKey = 0;
}

bool UChaosVDInstancedStaticMeshComponent::UpdateGeometryKey(const uint32 NewHandleGeometryKey)
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

TSharedPtr<FChaosVDMeshDataInstanceHandle> UChaosVDInstancedStaticMeshComponent::AddMeshInstance(const FTransform InstanceTransform, bool bIsWorldSpace, const TSharedPtr<FChaosVDExtractedGeometryDataHandle>& InGeometryHandle, int32 ParticleID, int32 SolverID)
{
	int32 InstanceIndex = AddInstance(InstanceTransform, bIsWorldSpace);

	check(InstanceIndex != INDEX_NONE);

	const TSharedPtr<FChaosVDMeshDataInstanceHandle> InstanceHandle = MakeShared<FChaosVDMeshDataInstanceHandle>(InstanceIndex, this, ParticleID, SolverID);
	InstanceHandle->SetGeometryHandle(InGeometryHandle);

	const uint32 NewHandleGeometryKey = InGeometryHandle->GetGeometryKey();

	if (!UpdateGeometryKey(NewHandleGeometryKey))
	{
		return nullptr;
	}
	
	CurrentInstanceHandles.Add(InstanceHandle);

	return InstanceHandle;
}

void UChaosVDInstancedStaticMeshComponent::AddMeshInstanceForHandle(TSharedPtr<FChaosVDMeshDataInstanceHandle> MeshDataHandle, const FTransform InstanceTransform, bool bIsWorldSpace, const TSharedPtr<FChaosVDExtractedGeometryDataHandle>& InGeometryHandle, int32 ParticleID, int32 SolverID)
{
	const int32 InstanceIndex = AddInstance(InstanceTransform, bIsWorldSpace);

	check(InstanceIndex != INDEX_NONE);

	const uint32 NewHandleGeometryKey = InGeometryHandle->GetGeometryKey();

	if (!UpdateGeometryKey(NewHandleGeometryKey))
	{
		return;
	}

	MeshDataHandle->SetMeshInstanceIndex(InstanceIndex);
	MeshDataHandle->SetMeshComponent(this);
	
	CurrentInstanceHandles.Add(MeshDataHandle);
}

void UChaosVDInstancedStaticMeshComponent::RemoveMeshInstance(TSharedPtr<FChaosVDMeshDataInstanceHandle> InHandleToRemove)
{
	if (!InHandleToRemove.IsValid())
	{
		UE_LOG(LogChaosVDEditor, Warning, TEXT("[%s] Attempted to remove a mesh instace using an invalid handle. No instanced were removed"), ANSI_TO_TCHAR(__FUNCTION__));
		return;
	}

	if (InHandleToRemove->GetMeshComponent() != this)
	{
		UE_LOG(LogChaosVDEditor, Error, TEXT("[%s] Attempted to remove a mesh instace using a handle from another component. No instanced were removed | Handle Component [%s] | Current Component [%s]"), ANSI_TO_TCHAR(__FUNCTION__), *GetNameSafe(InHandleToRemove->GetMeshComponent()), *GetNameSafe(this));
		return;
	}

	if (!ensure(InHandleToRemove->GetMeshInstanceIndex() != INDEX_NONE))
	{
		return;
	}

	{
		const int32 CurrentInstanceCount =  GetInstanceCount();

		if (InHandleToRemove->GetMeshInstanceIndex() > CurrentInstanceCount - 1)
		{
			ensure(false);

			UE_LOG(LogChaosVDEditor, Error, TEXT("[%s] Mesh Data Handle has an invalid instance index. No instanced were removed | Handle Instance Index [%d] | Current Instance Count [%d]"), ANSI_TO_TCHAR(__FUNCTION__), InHandleToRemove->GetMeshInstanceIndex(), CurrentInstanceCount);
			return;
		}
	}

	RemoveInstance(InHandleToRemove->GetMeshInstanceIndex());

	auto FindPredicate = [InHandleToRemove](const TSharedPtr<FChaosVDMeshDataInstanceHandle>& InstanceHandle)
	{
		return InstanceHandle->GetMeshInstanceIndex() == InHandleToRemove->GetMeshInstanceIndex();
	};

	// TODO: Evaluate if keeping the instance handles array sorted on any change is worth the cost to allow a binary search here. Same question for a Set
	const int32 InstanceHandleIndex = CurrentInstanceHandles.IndexOfByPredicate(FindPredicate);

	if (InstanceHandleIndex != INDEX_NONE)
	{
		CurrentInstanceHandles.RemoveAtSwap(InstanceHandleIndex);
	}

	// If the component becomes "empty", allow it to go back to the pool
	if (GetInstanceCount() == 0)
	{
		ComponentEmptyDelegate.Broadcast(this);
	}
}

bool UChaosVDInstancedStaticMeshComponent::Modify(bool bAlwaysMarkDirty)
{
	// CVD Mesh Components are not saved to any assets or require undo
	return false;
}

bool UChaosVDInstancedStaticMeshComponent::IsNavigationRelevant() const
{
	return false;
}

uint32 UChaosVDInstancedStaticMeshComponent::GetGeometryKey() const
{
	return CurrentGeometryKey;
}

TSharedPtr<FChaosVDMeshDataInstanceHandle> UChaosVDInstancedStaticMeshComponent::GetMeshDataInstanceHandle(int32 InstanceIndex) const
{
	auto FindPredicate = [InstanceIndex](const TSharedPtr<FChaosVDMeshDataInstanceHandle>& InstanceHandle)
	{
		return InstanceHandle->GetMeshInstanceIndex() == InstanceIndex;
	};

	if (const TSharedPtr<FChaosVDMeshDataInstanceHandle>* FoundHandle = CurrentInstanceHandles.FindByPredicate(FindPredicate))
	{
		return *FoundHandle;
	}

	return nullptr;
}
