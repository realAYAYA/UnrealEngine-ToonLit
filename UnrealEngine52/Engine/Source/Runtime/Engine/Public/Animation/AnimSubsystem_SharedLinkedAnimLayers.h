// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"
#include "Animation/AnimSubsystemInstance.h"
#include "AnimSubsystem_SharedLinkedAnimLayers.generated.h"

class UAnimInstance;
class USkeletalMeshComponent;

// Define to 1 for more extensive runtime validation of linked anim layers data
#define LINKEDANIMLAYERSDATA_INTEGRITYCHECKS 0

// Linked layer instance info
USTRUCT()
struct FLinkedAnimLayerInstanceData
{
	GENERATED_USTRUCT_BODY()

	FLinkedAnimLayerInstanceData() {}
	FLinkedAnimLayerInstanceData(UAnimInstance* AnimInstance) { Instance = AnimInstance; }

	// Mark a function as linked
	void AddLinkedFunction(FName Function, UAnimInstance* AnimInstance);
	// Unmark a function as linked
	void RemoveLinkedFunction(FName Function);

	// Return map of currently linked functions
	const TMap<FName, TWeakObjectPtr<UAnimInstance>>& GetLinkedFunctions() const {return LinkedFunctions;}
	
	UPROPERTY(Transient)
	TObjectPtr<UAnimInstance> Instance;

private:
	UPROPERTY(Transient)
	TMap<FName, TWeakObjectPtr<UAnimInstance>> LinkedFunctions;
};

// Linked layer class info
USTRUCT()
struct FLinkedAnimLayerClassData
{
	GENERATED_BODY()

	FLinkedAnimLayerClassData() {}
	FLinkedAnimLayerClassData(TSubclassOf<UAnimInstance> AnimClass) { Class = AnimClass; }

	// Find instance data for given instance if it exists, returns nullptr otherwise
	FLinkedAnimLayerInstanceData* FindInstanceData(const UAnimInstance* AnimInstance);

	// Find an existing instance to link given function if one is available, returns nullptr otherwise
	FLinkedAnimLayerInstanceData* FindInstanceForLinking(FName Function);
	
	// Add given instance
	FLinkedAnimLayerInstanceData& AddInstance(UAnimInstance* AnimInstance);
	// Remove given instance
	void RemoveInstance(const UAnimInstance* AnimInstance);

	TSubclassOf<UAnimInstance> GetClass() const {return Class;}
	const TArray<FLinkedAnimLayerInstanceData>& GetInstancesData() const {return InstancesData;}

private:
	TSubclassOf<UAnimInstance> Class;

	UPROPERTY(Transient)
	TArray<FLinkedAnimLayerInstanceData> InstancesData;
};

// Data for shared linked anim instances module
USTRUCT()
struct ENGINE_API FAnimSubsystem_SharedLinkedAnimLayers : public FAnimSubsystemInstance
{
	GENERATED_BODY()

	// Retrieve subsystem from skeletal mesh component
	static FAnimSubsystem_SharedLinkedAnimLayers* GetFromMesh(USkeletalMeshComponent* SkelMesh);
	
	// Clear all linked layers data
	void Reset();

	// Find instance data for given instance if it exists, returns nullptr otherwise
	FLinkedAnimLayerInstanceData* FindInstanceData(const UAnimInstance* AnimInstance);

	// Find class data for given anim class if it exists, returns nullptr otherwise
	FLinkedAnimLayerClassData* FindClassData(TSubclassOf<UAnimInstance> AnimClass);

	// Find or add ClassData for given AnimClass
	FLinkedAnimLayerClassData& FindOrAddClassData(TSubclassOf<UAnimInstance> AnimClass);

	// Remove given instance
	void RemoveInstance(const UAnimInstance* AnimInstance);

	const TArray<FLinkedAnimLayerClassData>& GetClassesData() const { return ClassesData; }
private:
	UPROPERTY(Transient)
	TArray<FLinkedAnimLayerClassData> ClassesData;
};