// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/LightWeightInstanceManager.h"
#include "Elements/SMInstance/SMInstanceManager.h"

#include "LightWeightInstanceStaticMeshManager.generated.h"

DECLARE_DYNAMIC_DELEGATE_RetVal_OneParam(FActorInstanceHandle, FOnActorReady, FActorInstanceHandle, InHandle);

UCLASS(BlueprintType, Blueprintable)
class ENGINE_API ALightWeightInstanceStaticMeshManager : public ALightWeightInstanceManager, public ISMInstanceManager
{
	GENERATED_UCLASS_BODY()

	virtual void SetRepresentedClass(UClass* ActorClass) override;

	// Sets the static mesh to use based on the info contained in InActor
	virtual void SetStaticMeshFromActor(AActor* InActor);

	// Clears the static mesh used for rendering instances
	void ClearStaticMesh();

	virtual int32 ConvertCollisionIndexToLightWeightIndex(int32 InIndex) const override;

	virtual int32 ConvertLightWeightIndexToCollisionIndex(int32 InIndex) const override;

protected:
	virtual void AddNewInstanceAt(FLWIData* InitData, int32 Index) override;

	virtual void RemoveInstance(int32 Index) override;

	void RemoveInstanceFromRendering(int32 DataIndex);

	void PostRemoveInstanceFromRendering();

	// sets the parameters on the instanced static mesh component
	virtual void SetInstancedStaticMeshParams();

	// Called when we set the static mesh
	void OnStaticMeshSet();

	virtual void OnRep_Transforms() override;

	virtual void PostActorSpawn(const FActorInstanceHandle& Handle) override;

public:

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:

	//~ ISMInstanceManager interface
	virtual FText GetSMInstanceDisplayName(const FSMInstanceId& InstanceId) const override;
	virtual FText GetSMInstanceTooltip(const FSMInstanceId& InstanceId) const override;
	virtual bool CanEditSMInstance(const FSMInstanceId& InstanceId) const override;
	virtual bool CanMoveSMInstance(const FSMInstanceId& InstanceId, const ETypedElementWorldType InWorldType) const override;
	virtual bool GetSMInstanceTransform(const FSMInstanceId& InstanceId, FTransform& OutInstanceTransform, bool bWorldSpace = false) const override;
	virtual bool SetSMInstanceTransform(const FSMInstanceId& InstanceId, const FTransform& InstanceTransform, bool bWorldSpace = false, bool bMarkRenderStateDirty = false, bool bTeleport = false) override;
	virtual void NotifySMInstanceMovementStarted(const FSMInstanceId& InstanceId) override;
	virtual void NotifySMInstanceMovementOngoing(const FSMInstanceId& InstanceId) override;
	virtual void NotifySMInstanceMovementEnded(const FSMInstanceId& InstanceId) override;
	virtual void NotifySMInstanceSelectionChanged(const FSMInstanceId& InstanceId, const bool bIsSelected) override;
	virtual bool DeleteSMInstances(TArrayView<const FSMInstanceId> InstanceIds) override;
	virtual bool DuplicateSMInstances(TArrayView<const FSMInstanceId> InstanceIds, TArray<FSMInstanceId>& OutNewInstanceIds) override;

	virtual void DuplicateLWIInstances(TArrayView<const int32> DataIndices, TArray<int32>& OutNewDataIndices);
	void GetLWIDataIndices(TArrayView<const FSMInstanceId> InstanceIds, TArray<int32>& OutDataIndices) const;

	virtual int32 ConvertInternalIndexToHandleIndex(int32 InternalIndex) const override;
	virtual int32 ConvertHandleIndexToInternalIndex(int32 HandleIndex) const override;

protected:

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = Debug, ReplicatedUsing = OnRep_StaticMesh)
	TSoftObjectPtr<UStaticMesh> StaticMesh;
	UFUNCTION()
	void OnRep_StaticMesh();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Debug, AdvancedDisplay, meta = (BlueprintProtected = "true", AllowPrivateAccess = "true"))
	TObjectPtr<class UHierarchicalInstancedStaticMeshComponent> InstancedStaticMeshComponent;

	//
	// Bookkeeping info
	//

	// keep track of the relationship between our data and the rendering data stored in the instanced static mesh component
	UPROPERTY(Replicated)
	TArray<int32> RenderingIndicesToDataIndices;
	UPROPERTY(Replicated)
	TArray<int32> DataIndicesToRenderingIndices;

	// Data indices that we are going to delete later in the frame, we impose a small delay to group deletions to avoid cases where we might try to delete the same index multiple times in a frame.
	TArray<int32> DataIndicesToBeDeleted;
};
