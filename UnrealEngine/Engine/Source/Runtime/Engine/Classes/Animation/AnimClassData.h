// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/AnimTypes.h"
#include "Animation/AnimStateMachineTypes.h"
#include "Animation/AnimClassInterface.h"
#include "Algo/Transform.h"
#include "AnimBlueprintGeneratedClass.h"
#include "PropertyAccess.h"

#include "AnimClassData.generated.h"

class USkeleton;

// @todo: BP2CPP_remove
/** Serialized anim BP function data */
USTRUCT()
struct UE_DEPRECATED(5.0, "This type is no longer in use and will be removed.") FAnimBlueprintFunctionData
{
	GENERATED_BODY()

	UPROPERTY()
	TFieldPath<FStructProperty> OutputPoseNodeProperty;

	/** The properties of the input nodes, patched up during link */
	UPROPERTY()
	TArray<TFieldPath<FStructProperty>> InputPoseNodeProperties;

	/** The input properties themselves */
	UPROPERTY()
	TArray<TFieldPath<FProperty>> InputProperties;
};

// @todo: BP2CPP_remove
class UE_DEPRECATED(5.0, "This type is no longer in use and will be removed.") UAnimClassData;
UCLASS()
class ENGINE_API UAnimClassData : public UObject, public IAnimClassInterface
{
	GENERATED_BODY()
public:
	// List of state machines present in this blueprint class
	UPROPERTY()
	TArray<FBakedAnimationStateMachine> BakedStateMachines;

	/** Target skeleton for this blueprint class */
	UPROPERTY()
	TObjectPtr<class USkeleton> TargetSkeleton;

	/** A list of anim notifies that state machines (or anything else) may reference */
	UPROPERTY()
	TArray<FAnimNotifyEvent> AnimNotifies;
	
	// Indices for each of the saved pose nodes that require updating, in the order they need to get updates.
	UPROPERTY()
	TMap<FName, FCachedPoseIndices> OrderedSavedPoseIndicesMap;

	// All of the functions that this anim class provides
	UPROPERTY()
	TArray<FAnimBlueprintFunction> AnimBlueprintFunctions;
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	// Serialized function data, used to patch up transient data in AnimBlueprintFunctions
	UPROPERTY()
	TArray<FAnimBlueprintFunctionData> AnimBlueprintFunctionData;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	// The array of anim nodes
	UPROPERTY()
	TArray< TFieldPath<FStructProperty> > AnimNodeProperties;
	TArray< FStructProperty* > ResolvedAnimNodeProperties;

	// The array of linked anim graph nodes
	UPROPERTY()
	TArray< TFieldPath<FStructProperty> > LinkedAnimGraphNodeProperties;
	TArray< FStructProperty* > ResolvedLinkedAnimGraphNodeProperties;

	// The array of linked anim layer nodes
	UPROPERTY()
	TArray< TFieldPath<FStructProperty> > LinkedAnimLayerNodeProperties;
	TArray< FStructProperty* > ResolvedLinkedAnimLayerNodeProperties;

	// Array of nodes that need a PreUpdate() call
	UPROPERTY()
	TArray< TFieldPath<FStructProperty> > PreUpdateNodeProperties;
	TArray< FStructProperty* > ResolvedPreUpdateNodeProperties;

	// Array of nodes that need a DynamicReset() call
	UPROPERTY()
	TArray< TFieldPath<FStructProperty> > DynamicResetNodeProperties;
	TArray< FStructProperty* > ResolvedDynamicResetNodeProperties;

	// Array of state machine nodes
	UPROPERTY()
	TArray< TFieldPath<FStructProperty> > StateMachineNodeProperties;
	TArray< FStructProperty* > ResolvedStateMachineNodeProperties;

	// Array of nodes that need an OnInitializeAnimInstance call
	UPROPERTY()
	TArray< TFieldPath<FStructProperty> > InitializationNodeProperties;
	TArray< FStructProperty* > ResolvedInitializationNodeProperties;

	// Indices for any Asset Player found within a specific (named) Anim Layer Graph, or implemented Anim Interface Graph
	UPROPERTY()
	TMap<FName, FGraphAssetPlayerInformation> GraphNameAssetPlayers;

	// Array of sync group names in the order that they are requested during compile
	UPROPERTY()
	TArray<FName> SyncGroupNames;

	// Per layer graph blending options
	UPROPERTY()
	TMap<FName, FAnimGraphBlendOptions> GraphBlendOptions;

public:
	// IAnimClassInterface interface
	virtual const TArray<FBakedAnimationStateMachine>& GetBakedStateMachines() const override { return BakedStateMachines; }
	virtual USkeleton* GetTargetSkeleton() const override { return TargetSkeleton; }
	virtual const TArray<FAnimNotifyEvent>& GetAnimNotifies() const override { return AnimNotifies; }
	virtual const TArray<FAnimBlueprintFunction>& GetAnimBlueprintFunctions() const override { return AnimBlueprintFunctions; }
	virtual const TMap<FName, FCachedPoseIndices>& GetOrderedSavedPoseNodeIndicesMap() const override { return OrderedSavedPoseIndicesMap; }
	virtual const TArray<FStructProperty*>& GetAnimNodeProperties() const override { return ResolvedAnimNodeProperties; }
	virtual const TArray<FStructProperty*>& GetLinkedAnimGraphNodeProperties() const override { return ResolvedLinkedAnimGraphNodeProperties; }
	virtual const TArray<FStructProperty*>& GetLinkedAnimLayerNodeProperties() const override { return ResolvedLinkedAnimLayerNodeProperties; }
	virtual const TArray<FStructProperty*>& GetPreUpdateNodeProperties() const override { return ResolvedPreUpdateNodeProperties; }
	virtual const TArray<FStructProperty*>& GetDynamicResetNodeProperties() const override { return ResolvedDynamicResetNodeProperties; }
	virtual const TArray<FStructProperty*>& GetStateMachineNodeProperties() const override { return ResolvedStateMachineNodeProperties; }
	virtual const TArray<FStructProperty*>& GetInitializationNodeProperties() const override { return ResolvedInitializationNodeProperties; }
	virtual const TArray<FName>& GetSyncGroupNames() const override { return SyncGroupNames; }
	virtual int32 GetSyncGroupIndex(FName SyncGroupName) const override { return SyncGroupNames.IndexOfByKey(SyncGroupName); }
	virtual const TMap<FName, FGraphAssetPlayerInformation>& GetGraphAssetPlayerInformation() const override { return GraphNameAssetPlayers; }
	virtual const TMap<FName, FAnimGraphBlendOptions>& GetGraphBlendOptions() const override { return GraphBlendOptions; }

private:
	virtual const TArray<FBakedAnimationStateMachine>& GetBakedStateMachines_Direct() const override { return BakedStateMachines; }
	virtual const TArray<FAnimNotifyEvent>& GetAnimNotifies_Direct() const override { return AnimNotifies; }
	virtual const TArray<FName>& GetSyncGroupNames_Direct() const override { return SyncGroupNames; }
	virtual const TMap<FName, FCachedPoseIndices>& GetOrderedSavedPoseNodeIndicesMap_Direct() const override { return OrderedSavedPoseIndicesMap; }
	virtual const TMap<FName, FGraphAssetPlayerInformation>& GetGraphAssetPlayerInformation_Direct() const override { return GraphNameAssetPlayers; }
	virtual const TMap<FName, FAnimGraphBlendOptions>& GetGraphBlendOptions_Direct() const override { return GraphBlendOptions; }
	
private:
	virtual const void* GetConstantNodeValueRaw(int32 InIndex) const override { return nullptr; }
	virtual const void* GetMutableNodeValueRaw(int32 InIndex, const UObject* InObject) const override { return nullptr; }
	virtual const FAnimBlueprintMutableData* GetMutableNodeData(const UObject* InObject) const override { return nullptr; }
	virtual FAnimBlueprintMutableData* GetMutableNodeData(UObject* InObject) const override { return nullptr; }
	virtual const void* GetConstantNodeData() const override { return nullptr; }
	virtual TArrayView<const FAnimNodeData> GetNodeData() const override;
	virtual int32 GetAnimNodePropertyIndex(const UScriptStruct* InNodeType, FName InPropertyName) const override { return INDEX_NONE; }
	virtual int32 GetAnimNodePropertyCount(const UScriptStruct* InNodeType) const override { return 0; }
	
	virtual void ForEachSubsystem(TFunctionRef<EAnimSubsystemEnumeration(const FAnimSubsystemContext&)> InFunction) const override {}
	virtual void ForEachSubsystem(UObject* InObject, TFunctionRef<EAnimSubsystemEnumeration(const FAnimSubsystemInstanceContext&)> InFunction) const override {}
	virtual const FAnimSubsystem* FindSubsystem(UScriptStruct* InStruct) const override { return nullptr; }

#if WITH_EDITORONLY_DATA
	virtual bool IsDataLayoutValid() const override { return false; };
#endif
	
public:
#if WITH_EDITOR
	// Copy data from an existing BP generated class to this class data
	void CopyFrom(UAnimBlueprintGeneratedClass* AnimClass);
#endif // WITH_EDITOR
};