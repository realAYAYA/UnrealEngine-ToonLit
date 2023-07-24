// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimSubsystem_SharedLinkedAnimLayers.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimSubsystem_SharedLinkedAnimLayers)

#if LINKEDANIMLAYERSDATA_INTEGRITYCHECKS
#include "Animation/AnimClassInterface.h"
#include "Animation/AnimNode_LinkedAnimLayer.h"

namespace
{
	// Check that function is either linked (!bIsFree) or unlinked (bIsFree)
	void CheckLayerDataIntegrity(const UAnimInstance* AnimInstance, FName FunctionName, bool bIsFree)
	{
		bool bFound = false;

		TArray<const UAnimInstance*> AllAnimInstances;
		const USkeletalMeshComponent* SkelMesh = AnimInstance->GetSkelMeshComponent();
		AllAnimInstances.Add(SkelMesh->GetAnimInstance());
		AllAnimInstances.Append(SkelMesh->GetLinkedAnimInstances());
		for (const UAnimInstance* LinkedAnimInstance : AllAnimInstances)
		{
			const IAnimClassInterface* const NewLinkedInstanceClass = IAnimClassInterface::GetFromClass(LinkedAnimInstance->GetClass());
			for (const FStructProperty* const LayerNodeProperty : NewLinkedInstanceClass->GetLinkedAnimLayerNodeProperties())
			{
				const FAnimNode_LinkedAnimLayer* const LinkedAnimLayerNode = LayerNodeProperty->ContainerPtrToValuePtr<const FAnimNode_LinkedAnimLayer>(LinkedAnimInstance);
				if (LinkedAnimLayerNode->GetDynamicLinkFunctionName() == FunctionName)
				{
					if (LinkedAnimLayerNode->GetTargetInstance<const UAnimInstance>() == AnimInstance)
					{
						// Function should be free to link but isn't
						if (bIsFree)
						{
							check(0);
						}
						else
						{
							// Function shouldn't be linked more than once
							check(!bFound);
							bFound = true;
						}
					}
				}
			}
		}
		// Function is either found or free
		check(bIsFree || bFound);
	}
}

#endif // LINKEDANIMLAYERSDATA_INTEGRITYCHECKS

void FLinkedAnimLayerInstanceData::AddLinkedFunction(FName Function, UAnimInstance* AnimInstance)
{
#if LINKEDANIMLAYERSDATA_INTEGRITYCHECKS
	CheckLayerDataIntegrity(Instance, Function, true);
#endif
	check(!LinkedFunctions.Contains(Function));
	LinkedFunctions.Add(Function, AnimInstance);
}

void FLinkedAnimLayerInstanceData::RemoveLinkedFunction(FName Function)
{
#if LINKEDANIMLAYERSDATA_INTEGRITYCHECKS
	CheckLayerDataIntegrity(Instance, Function, true);
#endif
	check(LinkedFunctions.Contains(Function));
	LinkedFunctions.Remove(Function);
}
FLinkedAnimLayerInstanceData* FLinkedAnimLayerClassData::FindInstanceData(const UAnimInstance* AnimInstance)
{
	return InstancesData.FindByPredicate([AnimInstance](FLinkedAnimLayerInstanceData& InstanceData) {return InstanceData.Instance == AnimInstance; });
}

FLinkedAnimLayerInstanceData* FLinkedAnimLayerClassData::FindInstanceForLinking(FName Function)
{
	for (FLinkedAnimLayerInstanceData& LayerInstanceData : InstancesData)
	{
		// Check if function is already linked
		if (!LayerInstanceData.GetLinkedFunctions().Contains(Function))
		{
			// not linked, use this instance
			return &LayerInstanceData;
		}
	}
	return nullptr;
}

FLinkedAnimLayerInstanceData& FLinkedAnimLayerClassData::AddInstance(UAnimInstance* AnimInstance)
{
	InstancesData.Push(FLinkedAnimLayerInstanceData(AnimInstance));
	return InstancesData.Last();
}

void FLinkedAnimLayerClassData::RemoveInstance(const UAnimInstance* AnimInstance)
{
	for (int i = 0; i < InstancesData.Num(); ++i)
	{
		if (InstancesData[i].Instance == AnimInstance)
		{
			// Make sure no functions are still linked
			check(InstancesData[i].GetLinkedFunctions().Num() == 0);
			InstancesData.RemoveAt(i);
			return;
		}
	}
	check(0);
}

FAnimSubsystem_SharedLinkedAnimLayers* FAnimSubsystem_SharedLinkedAnimLayers::GetFromMesh(USkeletalMeshComponent* SkelMesh)
{
#if WITH_EDITOR
	if (GIsReinstancing)
	{
		return nullptr;
	}
#endif
	if (!SkelMesh)
	{
		return nullptr;
	}
	check(SkelMesh->GetAnimInstance());
	return SkelMesh->GetAnimInstance()->FindSubsystem<FAnimSubsystem_SharedLinkedAnimLayers>();
}

void FAnimSubsystem_SharedLinkedAnimLayers::Reset()
{
	ClassesData.Empty(ClassesData.Num());
}

FLinkedAnimLayerInstanceData* FAnimSubsystem_SharedLinkedAnimLayers::FindInstanceData(const UAnimInstance* AnimInstance)
{
	if (FLinkedAnimLayerClassData* ClassData = FindClassData(AnimInstance->GetClass()))
	{
		return ClassData->FindInstanceData(AnimInstance);
	}
	return nullptr;
}

FLinkedAnimLayerClassData* FAnimSubsystem_SharedLinkedAnimLayers::FindClassData(TSubclassOf<UAnimInstance> AnimClass)
{
	return ClassesData.FindByPredicate([AnimClass](const FLinkedAnimLayerClassData& ClassData) {return ClassData.GetClass() == AnimClass; });
}

FLinkedAnimLayerClassData& FAnimSubsystem_SharedLinkedAnimLayers::FindOrAddClassData(TSubclassOf<UAnimInstance> AnimClass)
{
	if (FLinkedAnimLayerClassData* Result = FindClassData(AnimClass))
	{
		return *Result;
	}
	ClassesData.Add(FLinkedAnimLayerClassData(AnimClass));
	return ClassesData.Last();
};

void FAnimSubsystem_SharedLinkedAnimLayers::RemoveInstance(const UAnimInstance* AnimInstance)
{
	int32 ClassIndex = ClassesData.IndexOfByPredicate([AnimInstance](const FLinkedAnimLayerClassData& ClassData) {return ClassData.GetClass() == AnimInstance->GetClass(); });
	if (ClassIndex != INDEX_NONE)
	{
		FLinkedAnimLayerClassData& ClassData = ClassesData[ClassIndex];
		ClassData.RemoveInstance(AnimInstance);
		// Remove class data when all instances are removed
		if (ClassData.GetInstancesData().Num() == 0)
		{
			// Remove class
			ClassesData.RemoveAt(ClassIndex);
		}
	}
	else
	{
		check(0);
	}
}
