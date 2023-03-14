// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "ActorPartition/PartitionActor.h"
#include "ISMPartition/ISMPartitionClient.h"
#include "ISMPartition/ISMPartitionInstanceManager.h"
#include "ISMPartition/ISMComponentDescriptor.h"
#include "ISMPartition/ISMComponentData.h"
#include "Templates/Tuple.h"
#include "Containers/SortedMap.h"
#include "Elements/SMInstance/SMInstanceManager.h"
#include "ISMPartitionActor.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogISMPartition, Log, All);

/** Actor base class for instance containers placed on a grid.
	See UActorPartitionSubsystem. */
UCLASS(Abstract)
class ENGINE_API AISMPartitionActor : public APartitionActor, public ISMInstanceManager, public ISMInstanceManagerProvider
{
	GENERATED_UCLASS_BODY()

public:
#if WITH_EDITOR	
	//~ Begin AActor Interface
	virtual void PreEditUndo() override;
	virtual void PostEditUndo() override;
	//~ End AActor Interface	

	FISMClientHandle RegisterClient(const FGuid& ClientGuid);
	void UnregisterClient(FISMClientHandle& ClientHandle);

	void RegisterClientInstanceManager(const FISMClientHandle& Handle, IISMPartitionInstanceManager* ClientInstanceManager);
	void RegisterClientInstanceManagerProvider(const FISMClientHandle& Handle, IISMPartitionInstanceManagerProvider* ClientInstanceManagerProvider);

	int32 RegisterISMComponentDescriptor(const FISMComponentDescriptor& Descriptor);
	const FISMComponentDescriptor& GetISMComponentDescriptor(int32 DescriptorIndex) const { return Descriptors[DescriptorIndex]; }

	TArray<FSMInstanceId> AddISMInstance(const FISMClientHandle& Handle, const FTransform& InstanceTransform, const TSortedMap<int32, TArray<FTransform>>& InstanceDefinition);
	void RemoveISMInstance(const FISMClientHandle& Handle, int32 InstanceIndex, bool* bOutIsEmpty = nullptr);
	void RemoveISMInstances(const FISMClientHandle& Handle);
	void SelectISMInstances(const FISMClientHandle& Handle, bool bSelect, const TSet<int32>& Indices);
	void SetISMInstanceTransform(const FISMClientHandle& Handle, int32 InstanceIndex, const FTransform& NewTransform, bool bTeleport, const TSortedMap<int32, TArray<FTransform>>& InstanceDefinition);
	int32 GetISMInstanceIndex(const FISMClientHandle& Handle, const UInstancedStaticMeshComponent* ISMComponent, int32 ComponentIndex) const;
	FBox GetISMInstanceBounds(const FISMClientHandle& Handle, const TSet<int32>& Indices) const;
	void ReserveISMInstances(const FISMClientHandle& Handle, int32 AddedInstanceCount, const TSortedMap<int32, TArray<FTransform>>& InstanceDefinition);

	bool IsISMComponent(const UPrimitiveComponent* Component) const;
	void BeginUpdate();
	void EndUpdate();
	void UpdateHISMTrees(bool bAsync, bool bForce);
	void ForEachClientComponent(const FISMClientHandle& Handle, TFunctionRef<bool(UInstancedStaticMeshComponent*)> Callback) const;
	void ForEachClientSMInstance(const FISMClientHandle& Handle, TFunctionRef<bool(FSMInstanceId)> Callback) const;
	void ForEachClientSMInstance(const FISMClientHandle& Handle, int32 InstanceIndex, TFunctionRef<bool(FSMInstanceId)> Callback) const;
	void OutputStats() const;

protected:

private:
	void RemoveISMInstancesInternal(FISMComponentData& ComponentData, FISMClientData& OwnerData, int32 InstanceIndex);

	void InvalidateComponentLightingCache(FISMComponentData& ComponentData);
	int32 AddInstanceToComponent(FISMComponentData& ComponentData, const FTransform& WorldTransform);
	void UpdateInstanceTransform(FISMComponentData& ComponentData, int32 ComponentInstanceIndex, const FTransform& WorldTransform, bool bTeleport);
	void RemoveInstanceFromComponent(FISMComponentData& ComponentData, int32 ComponentInstanceIndex);
	bool DestroyComponentIfEmpty(FISMComponentDescriptor& Descriptor, FISMComponentData& ComponentData);
	void ModifyComponent(FISMComponentData& ComponentData);
	void CreateComponent(const FISMComponentDescriptor& Descriptor, FISMComponentData& ComponentData);
	void ModifyActor();
#endif

private:
	//~ ISMInstanceManager interface
	virtual FText GetSMInstanceDisplayName(const FSMInstanceId& InstanceId) const override final;
	virtual FText GetSMInstanceTooltip(const FSMInstanceId& InstanceId) const override final;
	virtual bool CanEditSMInstance(const FSMInstanceId& InstanceId) const override final;
	virtual bool CanMoveSMInstance(const FSMInstanceId& InstanceId, const ETypedElementWorldType WorldType) const override final;
	virtual bool GetSMInstanceTransform(const FSMInstanceId& InstanceId, FTransform& OutInstanceTransform, bool bWorldSpace = false) const override final;
	virtual bool SetSMInstanceTransform(const FSMInstanceId& InstanceId, const FTransform& InstanceTransform, bool bWorldSpace = false, bool bMarkRenderStateDirty = false, bool bTeleport = false) override final;
	virtual void NotifySMInstanceMovementStarted(const FSMInstanceId& InstanceId) override final;
	virtual void NotifySMInstanceMovementOngoing(const FSMInstanceId& InstanceId) override final;
	virtual void NotifySMInstanceMovementEnded(const FSMInstanceId& InstanceId) override final;
	virtual void NotifySMInstanceSelectionChanged(const FSMInstanceId& InstanceId, const bool bIsSelected) override final;
	virtual void ForEachSMInstanceInSelectionGroup(const FSMInstanceId& InstanceId, TFunctionRef<bool(FSMInstanceId)> Callback) override final;
	virtual bool CanDeleteSMInstance(const FSMInstanceId& InstanceId) const override final;
	virtual bool DeleteSMInstances(TArrayView<const FSMInstanceId> InstanceIds) override final;
	virtual bool CanDuplicateSMInstance(const FSMInstanceId& InstanceId) const override final;
	virtual bool DuplicateSMInstances(TArrayView<const FSMInstanceId> InstanceIds, TArray<FSMInstanceId>& OutNewInstanceIds) override final;

#if WITH_EDITOR
	FISMPartitionInstanceManager GetISMPartitionInstanceManager(const FSMInstanceId& InstanceId) const;
	FISMPartitionInstanceManager GetISMPartitionInstanceManagerChecked(const FSMInstanceId& InstanceId) const;
#endif

protected:
	//~ ISMInstanceManagerProvider interface
	virtual ISMInstanceManager* GetSMInstanceManager(const FSMInstanceId& InstanceId) override /*final*/; // Note: This should also be final and private, but AInstancedFoliageActor needs to override it to support ISMC foliage

private:
#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TArray<FGuid> Clients;
		
	UPROPERTY()
	TArray<FISMComponentDescriptor> Descriptors;

	UPROPERTY()
	TArray<FISMComponentData> DescriptorComponents;

	UPROPERTY()
	TMap<FGuid, FISMClientInstanceManagerData> ClientInstanceManagers;

	/** If greater than 0 means we are between a BeginUpdate/EndUpdate call and there are some things we can delay/optimize */
	int32 UpdateDepth;

	/** If Modified as already been called between a BeginUpdate/EndUpdate (avoid multiple Modify calls on the component) */
	bool bWasModifyCalled;
#endif
};
