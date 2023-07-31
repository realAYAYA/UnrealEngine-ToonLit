// Copyright Epic Games, Inc. All Rights Reserved.

#include "InterchangeMeshActorFactoryNode.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InterchangeMeshActorFactoryNode)

#if WITH_ENGINE
	#include "GameFramework/Actor.h"
#endif

UInterchangeMeshActorFactoryNode::UInterchangeMeshActorFactoryNode()
{
	SlotMaterialDependencies.Initialize(Attributes.ToSharedRef(), TEXT("__SlotMaterialDependencies__"));
}

void UInterchangeMeshActorFactoryNode::GetSlotMaterialDependencies(TMap<FString, FString>& OutMaterialDependencies) const
{
	OutMaterialDependencies = SlotMaterialDependencies.ToMap();
}

bool UInterchangeMeshActorFactoryNode::GetSlotMaterialDependencyUid(const FString& SlotName, FString& OutMaterialDependency) const
{
	return SlotMaterialDependencies.GetValue(SlotName, OutMaterialDependency);
}

bool UInterchangeMeshActorFactoryNode::SetSlotMaterialDependencyUid(const FString& SlotName, const FString& MaterialDependencyUid)
{
	return SlotMaterialDependencies.SetKeyValue(SlotName, MaterialDependencyUid);
}

bool UInterchangeMeshActorFactoryNode::RemoveSlotMaterialDependencyUid(const FString& SlotName)
{
	return SlotMaterialDependencies.RemoveKey(SlotName);
}

