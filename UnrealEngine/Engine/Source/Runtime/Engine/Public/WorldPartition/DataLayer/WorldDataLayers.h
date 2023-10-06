// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GameFramework/Info.h"
#include "WorldPartition/DataLayer/ActorDataLayer.h"
#include "WorldPartition/DataLayer/DataLayerInstance.h"
#include "WorldPartition/DataLayer/DataLayer.h"
#include "Engine/World.h"
#include "WorldDataLayers.generated.h"

class UDEPRECATED_DataLayer;
class UDataLayerInstance;
class UDataLayerInstanceWithAsset;
class UDataLayerAsset;

USTRUCT()
struct FActorPlacementDataLayers
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FName> DataLayerInstanceNames;

	UPROPERTY()
	int32 ContextID = INT32_MAX;

	void Reset()
	{
		DataLayerInstanceNames.Reset();
	}
};

/**
 * Actor containing data layers instances within a world.
 */
UCLASS(hidecategories = (Actor, HLOD, Cooking, Transform, Advanced, Display, Events, Object, Physics, Attachment, Info, Input, Blueprint, Layers, Tags, Replication), notplaceable, MinimalAPI)
class AWorldDataLayers : public AInfo
{
	GENERATED_UCLASS_BODY()

	friend class UDataLayerToAssetCommandlet;

public:
	ENGINE_API virtual void PostLoad() override;
	ENGINE_API virtual void RewindForReplay() override;
	ENGINE_API virtual void BeginPlay() override;

#if WITH_EDITOR
	ENGINE_API virtual void PreEditUndo() override;
	ENGINE_API virtual void PostEditUndo() override;
	virtual bool ShouldLevelKeepRefIfExternal() const override { return true; }
	virtual bool ShouldImport(FStringView ActorPropString, bool IsMovingLevel) override { return false; }
	virtual bool IsLockLocation() const { return true; }
	virtual bool IsUserManaged() const override { return false; }
	virtual bool IsDataLayerTypeSupported(TSubclassOf<UDataLayerInstance> DataLayerType) const { return false; }
	ENGINE_API virtual TUniquePtr<class FWorldPartitionActorDesc> CreateClassActorDesc() const override;

	static ENGINE_API AWorldDataLayers* Create(UWorld* World, FName InWorldDataLayerName = NAME_None);
	static ENGINE_API AWorldDataLayers* Create(const FActorSpawnParameters& SpawnParameters);

	bool IsEmpty() const { return DataLayerInstances.IsEmpty(); }
	bool HasDeprecatedDataLayers() const { return bHasDeprecatedDataLayers; }

	template<class DataLayerInstanceType, typename ...CreationsArgs>
	DataLayerInstanceType* CreateDataLayer(CreationsArgs... InCreationArgs);

	ENGINE_API bool RemoveDataLayer(const UDataLayerInstance* InDataLayer);
	ENGINE_API int32 RemoveDataLayers(const TArray<UDataLayerInstance*>& InDataLayerInstances);
	ENGINE_API void SetAllowRuntimeDataLayerEditing(bool bInAllowRuntimeDataLayerEditing);
	bool GetAllowRuntimeDataLayerEditing() const { return bAllowRuntimeDataLayerEditing; }

	ENGINE_API bool IsInActorEditorContext(const UDataLayerInstance* InDataLayerInstance) const;
	ENGINE_API bool AddToActorEditorContext(UDataLayerInstance* InDataLayerInstance);
	ENGINE_API bool RemoveFromActorEditorContext(UDataLayerInstance* InDataLayerInstance);
	ENGINE_API void PushActorEditorContext(int32 InContextID);
	ENGINE_API void PopActorEditorContext(int32 InContextID);
	ENGINE_API TArray<UDataLayerInstance*> GetActorEditorContextDataLayers() const;

	//~ Begin Helper Functions
	ENGINE_API TArray<const UDataLayerInstance*> GetDataLayerInstances(const TArray<UDataLayerAsset*>& InDataLayersAssets) const;
	ENGINE_API TArray<const UDataLayerInstance*> GetDataLayerInstances(const TArray<const UDataLayerAsset*>& InDataLayersAssets) const;
	ENGINE_API TArray<FName> GetDataLayerInstanceNames(const TArray<const UDataLayerAsset*>& InDataLayersAssets) const;
	//~ End Helper Functions

	// Allows overriding of DataLayers with PlayFromHere
	template<class T>
	void OverwriteDataLayerRuntimeStates(const TArray<T>* InActiveDataLayers, const TArray<T>* InLoadedDataLayers );

	ENGINE_API bool IsReadOnly() const;
	ENGINE_API bool IsSubWorldDataLayers() const;
	ENGINE_API bool IsRuntimeRelevant() const;
#endif

	static FName GetWorldPartionWorldDataLayersName() { return FName(TEXT("WorldDataLayers")); } // reserved for ULevel::WorldDataLayers
	
	ENGINE_API void DumpDataLayers(FOutputDevice& OutputDevice) const;
	ENGINE_API bool ContainsDataLayer(const UDataLayerInstance* InDataLayer) const;
	ENGINE_API const UDataLayerInstance* GetDataLayerInstance(const FName& InDataLayerInstanceName) const;
	ENGINE_API const UDataLayerInstance* GetDataLayerInstance(const UDataLayerAsset* InDataLayerAsset) const;
	ENGINE_API const UDataLayerInstance* GetDataLayerInstanceFromAssetName(const FName& InDataLayerAssetFullName) const;
	ENGINE_API void ForEachDataLayer(TFunctionRef<bool(UDataLayerInstance*)> Func);
	ENGINE_API void ForEachDataLayer(TFunctionRef<bool(UDataLayerInstance*)> Func) const;

	ENGINE_API TArray<const UDataLayerInstance*> GetDataLayerInstances(const TArray<FName>& InDataLayerInstanceNames) const;

	// DataLayer Runtime State
	ENGINE_API void SetDataLayerRuntimeState(const UDataLayerInstance* InDataLayerInstance, EDataLayerRuntimeState InState, bool bIsRecursive = false);
	ENGINE_API EDataLayerRuntimeState GetDataLayerRuntimeStateByName(FName InDataLayerName) const;
	ENGINE_API EDataLayerRuntimeState GetDataLayerEffectiveRuntimeStateByName(FName InDataLAyerName) const;
	const TSet<FName>& GetEffectiveActiveDataLayerNames() const { return EffectiveActiveDataLayerNames; }
	const TSet<FName>& GetEffectiveLoadedDataLayerNames() const { return EffectiveLoadedDataLayerNames; }
	UFUNCTION(NetMulticast, Reliable)
	ENGINE_API void OnDataLayerRuntimeStateChanged(const UDataLayerInstance* InDataLayer, EDataLayerRuntimeState InState);
	static int32 GetDataLayersStateEpoch() { return DataLayersStateEpoch; }

	//~ Begin Deprecated

	PRAGMA_DISABLE_DEPRECATION_WARNINGS // Suppress compiler warning on override of deprecated function

	UE_DEPRECATED(5.0, "Use SetDataLayerRuntimeState() instead.")
	void SetDataLayerState(FActorDataLayer InDataLayer, EDataLayerState InState) { SetDataLayerRuntimeState(GetDataLayerInstance(InDataLayer.Name), (EDataLayerRuntimeState)InState); }

	UE_DEPRECATED(5.0, "Use GetDataLayerRuntimeStateByName() instead.")
	EDataLayerState GetDataLayerStateByName(FName InDataLayerName) const { return (EDataLayerState)GetDataLayerRuntimeStateByName(InDataLayerName); }

	UE_DEPRECATED(5.0, "Use GetEffectiveActiveDataLayerNames() instead.")
	const TSet<FName>& GetActiveDataLayerNames() const { return GetEffectiveActiveDataLayerNames(); }

	UE_DEPRECATED(5.0, "Use GetEffectiveLoadedDataLayerNames() instead.")
	const TSet<FName>& GetLoadedDataLayerNames() const { return GetEffectiveLoadedDataLayerNames(); }

#if WITH_EDITOR
	UE_DEPRECATED(5.1, "Convert DataLayer using UDataLayerToAssetCommandlet and use UDataLayerInstance* overload instead.")
	ENGINE_API bool RemoveDataLayer(const UDEPRECATED_DataLayer* InDataLayer);
#endif

	UE_DEPRECATED(5.1, "Convert DataLayer using UDataLayerToAssetCommandlet and use UDataLayerInstance* overload instead.")
	ENGINE_API bool ContainsDataLayer(const UDEPRECATED_DataLayer* InDataLayer) const;

	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	UE_DEPRECATED(5.1, "Use GetDataLayerInstance with FName or UDataLayerAsset instead")
	ENGINE_API const UDataLayerInstance* GetDataLayerInstance(const FActorDataLayer& InActorDataLayer) const;

	UE_DEPRECATED(5.1, "Use GetDataLayerInstanceNames with UDataLayerAsset instead")
	ENGINE_API TArray<FName> GetDataLayerInstanceNames(const TArray<FActorDataLayer>& InActorDataLayers) const;

	UE_DEPRECATED(5.1, "Use GetDataLayerInstances with FName or UDataLayerAsset instead")
	ENGINE_API TArray<const UDataLayerInstance*> GetDataLayerInstances(const TArray<FActorDataLayer>& InActorDataLayers) const;

	UE_DEPRECATED(5.1, "Convert UDataLayers to UDataLayerAsset and UDataLayerInstance using DataLayerToAssetCommandLet")
	ENGINE_API const UDataLayerInstance* GetDataLayerFromLabel(const FName& InDataLayerLabel) const;

	UE_DEPRECATED(5.3, "This function has been deprecated.")
	void GetUserLoadedInEditorStates(TArray<FName>& OutDataLayersLoadedInEditor, TArray<FName>& OutDataLayersNotLoadedInEditor) const {}

	//~ End Deprecated

protected:
	ENGINE_API void InitializeDataLayerRuntimeStates();
	ENGINE_API void ResetDataLayerRuntimeStates();

	UFUNCTION()
	ENGINE_API void OnRep_ActiveDataLayerNames();

	UFUNCTION()
	ENGINE_API void OnRep_LoadedDataLayerNames();

	UFUNCTION()
	ENGINE_API void OnRep_EffectiveActiveDataLayerNames();

	UFUNCTION()
	ENGINE_API void OnRep_EffectiveLoadedDataLayerNames();

private:
	ENGINE_API void ResolveEffectiveRuntimeState(const UDataLayerInstance* InDataLayer, bool bInNotifyChange = true);
	ENGINE_API void DumpDataLayerRecursively(const UDataLayerInstance* DataLayer, FString Prefix, FOutputDevice& OutputDevice) const;

#if WITH_EDITOR
	virtual bool ActorTypeIsMainWorldOnly() const override { return true; }

	ENGINE_API void ConvertDataLayerToInstancces();
	ENGINE_API void UpdateContainsDeprecatedDataLayers();
	ENGINE_API void ResolveActorDescContainers();
	friend class UDataLayerInstanceWithAsset;

	// Used to compare state pre/post undo
	TSet<TObjectPtr<UDataLayerInstance>> CachedDataLayerInstances;
#endif

#if !WITH_EDITOR
	TMap<FName, const UDataLayerInstance*> InstanceNameToInstance;
	TMap<FString, const UDataLayerInstance*> AssetNameToInstance;
#endif

#if WITH_EDITORONLY_DATA
	// True when Runtime Data Layer editing is allowed.
	UPROPERTY()
	bool bAllowRuntimeDataLayerEditing;

	UPROPERTY(Transient)
	FActorPlacementDataLayers CurrentDataLayers;

	TArray<FActorPlacementDataLayers> CurrentDataLayersStack;
#endif

	UPROPERTY()
	TSet<TObjectPtr<UDataLayerInstance>> DataLayerInstances;

	static_assert(DATALAYER_TO_INSTANCE_RUNTIME_CONVERSION_ENABLED, "DeprecatedDataLayerNameToDataLayerInstance Property is deprecated and needs to be deleted.");
	UPROPERTY()
	TMap<FName, TWeakObjectPtr<UDataLayerInstance>> DeprecatedDataLayerNameToDataLayerInstance;

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Convert Data layers to DataLayerInstances and DataLayerAssets using DataLayerToAsset CommandLet and use DataLayerInstances instead."))
	TSet<TObjectPtr<UDEPRECATED_DataLayer>> WorldDataLayers_DEPRECATED;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	UPROPERTY(Transient, Replicated, ReplicatedUsing=OnRep_ActiveDataLayerNames)
	TArray<FName> RepActiveDataLayerNames;
		
	UPROPERTY(Transient, Replicated, ReplicatedUsing=OnRep_LoadedDataLayerNames)
	TArray<FName> RepLoadedDataLayerNames;

	// TSet do not support replication so we replicate an array and update the set in the OnRep_ActiveDataLayerNames/OnRep_LoadedDataLayerNames
	TSet<FName> ActiveDataLayerNames;
	TSet<FName> LoadedDataLayerNames;

	UPROPERTY(Transient, Replicated, ReplicatedUsing=OnRep_EffectiveActiveDataLayerNames)
	TArray<FName> RepEffectiveActiveDataLayerNames;
		
	UPROPERTY(Transient, Replicated, ReplicatedUsing=OnRep_EffectiveLoadedDataLayerNames)
	TArray<FName> RepEffectiveLoadedDataLayerNames;

	// TSet do not support replication so we replicate an array and update the set in the OnRep_EffectiveActiveDataLayerNames/OnRep_EffectiveLoadedDataLayerNames
	TSet<FName> EffectiveActiveDataLayerNames;
	TSet<FName> EffectiveLoadedDataLayerNames;

	static ENGINE_API int32 DataLayersStateEpoch;

	static_assert(DATALAYER_TO_INSTANCE_RUNTIME_CONVERSION_ENABLED, "bHasDeprecatedDataLayers is deprecated and needs to be deleted.");
	bool bHasDeprecatedDataLayers;

public:
	DECLARE_DELEGATE_RetVal_ThreeParams(bool, FDataLayersFilterDelegate, FName /*DataLayerName*/, EDataLayerRuntimeState /*CurrentState*/, EDataLayerRuntimeState /*TargetState*/);

	UE_DEPRECATED(5.0, "do not use, will be replaced by another mechanism for initial release.")
	FDataLayersFilterDelegate DataLayersFilterDelegate;
};

DEFINE_ACTORDESC_TYPE(AWorldDataLayers, FWorldDataLayersActorDesc);

#if WITH_EDITOR

template <typename T, typename = int>
struct HasDataLayerInstanceClass : std::false_type {};

template <typename T>
struct HasDataLayerInstanceClass<T, decltype(&T::GetDataLayerInstanceClass, 0)> : std::true_type {};

template<class DataLayerInstanceType, typename ...CreationsArgs>
DataLayerInstanceType* AWorldDataLayers::CreateDataLayer(CreationsArgs... InCreationArgs)
{
	Modify();

	UClass* ClassToUse = DataLayerInstanceType::StaticClass();
	if constexpr (HasDataLayerInstanceClass<DataLayerInstanceType>::value)
		ClassToUse = DataLayerInstanceType::GetDataLayerInstanceClass();

	DataLayerInstanceType* NewDataLayer = NewObject<DataLayerInstanceType>(this, ClassToUse, DataLayerInstanceType::MakeName(Forward<CreationsArgs>(InCreationArgs)...), RF_Transactional | RF_NoFlags);
	check(NewDataLayer != nullptr);
	NewDataLayer->OnCreated(Forward<CreationsArgs>(InCreationArgs)...);
	DataLayerInstances.Add(NewDataLayer);

	UpdateContainsDeprecatedDataLayers();
	ResolveActorDescContainers();
	return NewDataLayer;
}

template<class IdentifierType>
void AWorldDataLayers::OverwriteDataLayerRuntimeStates(const TArray<IdentifierType>* InActiveDataLayers, const TArray<IdentifierType>* InLoadedDataLayers)
{
	if (GetLocalRole() == ROLE_Authority)
	{
		// This should get called before game starts. It doesn't send out events
		check(!GetWorld()->bMatchStarted);

		if (InActiveDataLayers)
		{
			ActiveDataLayerNames.Empty(InActiveDataLayers->Num());
			for (const IdentifierType& DataLayerIdentitier : *InActiveDataLayers)
			{
				PRAGMA_DISABLE_DEPRECATION_WARNINGS
				const UDataLayerInstance* DataLayerInstance = GetDataLayerInstance(DataLayerIdentitier);
				PRAGMA_ENABLE_DEPRECATION_WARNINGS
				if (DataLayerInstance && DataLayerInstance->IsRuntime())
				{
					ActiveDataLayerNames.Add(DataLayerInstance->GetDataLayerFName());
				}
			}
			RepActiveDataLayerNames = ActiveDataLayerNames.Array();
		}
		

		if(InLoadedDataLayers)
		{
			LoadedDataLayerNames.Empty(InLoadedDataLayers->Num());
			for (const IdentifierType& DataLayerIdentitier : *InLoadedDataLayers)
			{
				PRAGMA_DISABLE_DEPRECATION_WARNINGS
				const UDataLayerInstance* DataLayerInstance = GetDataLayerInstance(DataLayerIdentitier);
				PRAGMA_ENABLE_DEPRECATION_WARNINGS
				if (DataLayerInstance && DataLayerInstance->IsRuntime())
				{
					LoadedDataLayerNames.Add(DataLayerInstance->GetDataLayerFName());
				}
			}
			RepLoadedDataLayerNames = LoadedDataLayerNames.Array();
		}

		ForEachDataLayer([this](class UDataLayerInstance* DataLayer)
		{
			if (DataLayer && DataLayer->IsRuntime())
			{
				ResolveEffectiveRuntimeState(DataLayer, /*bNotifyChange*/false);
			}
			return true;
		});

		RepEffectiveActiveDataLayerNames = EffectiveActiveDataLayerNames.Array();
		RepEffectiveLoadedDataLayerNames = EffectiveLoadedDataLayerNames.Array();
	}
}

#endif // WITH_EDITOR
