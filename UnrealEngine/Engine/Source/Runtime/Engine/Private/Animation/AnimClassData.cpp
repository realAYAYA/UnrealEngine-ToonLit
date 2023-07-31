// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimClassData.h"
#include "Animation/AnimNode_Root.h"
#include "PropertyAccess.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimClassData)

#if WITH_EDITOR
PRAGMA_DISABLE_DEPRECATION_WARNINGS
void UAnimClassData::CopyFrom(UAnimBlueprintGeneratedClass* AnimClass)
{
	check(AnimClass);
	BakedStateMachines = AnimClass->GetBakedStateMachines();
	TargetSkeleton = AnimClass->GetTargetSkeleton();
	AnimNotifies = AnimClass->GetAnimNotifies();
	AnimBlueprintFunctions = AnimClass->GetAnimBlueprintFunctions();
	AnimBlueprintFunctionData.Empty(AnimBlueprintFunctions.Num());

	for(const FAnimBlueprintFunction& AnimBlueprintFunction : AnimBlueprintFunctions)
	{
		FAnimBlueprintFunctionData& NewAnimBlueprintFunctionData = AnimBlueprintFunctionData.AddDefaulted_GetRef();
		NewAnimBlueprintFunctionData.OutputPoseNodeProperty = AnimBlueprintFunction.OutputPoseNodeProperty;
		Algo::Transform(AnimBlueprintFunction.InputPoseNodeProperties, NewAnimBlueprintFunctionData.InputPoseNodeProperties, [](FStructProperty* InProperty){ return TFieldPath<FStructProperty>(InProperty); });
	}

	OrderedSavedPoseIndicesMap = AnimClass->GetOrderedSavedPoseNodeIndicesMap();

	auto MakePropertyPath = [](FStructProperty* InProperty)
	{ 
		return TFieldPath<FStructProperty>(InProperty); 
	};

	Algo::Transform(AnimClass->GetAnimNodeProperties(), AnimNodeProperties, MakePropertyPath);
	ResolvedAnimNodeProperties = AnimClass->GetAnimNodeProperties();
	Algo::Transform(AnimClass->GetLinkedAnimGraphNodeProperties(), LinkedAnimGraphNodeProperties, MakePropertyPath);
	ResolvedLinkedAnimGraphNodeProperties = AnimClass->GetLinkedAnimGraphNodeProperties();
	Algo::Transform(AnimClass->GetLinkedAnimLayerNodeProperties(), LinkedAnimLayerNodeProperties, MakePropertyPath);
	ResolvedLinkedAnimLayerNodeProperties = AnimClass->GetLinkedAnimLayerNodeProperties();
	Algo::Transform(AnimClass->GetPreUpdateNodeProperties(), PreUpdateNodeProperties, MakePropertyPath);
	ResolvedPreUpdateNodeProperties = AnimClass->GetPreUpdateNodeProperties();
	Algo::Transform(AnimClass->GetDynamicResetNodeProperties(), DynamicResetNodeProperties, MakePropertyPath);
	ResolvedDynamicResetNodeProperties = AnimClass->GetDynamicResetNodeProperties();
	Algo::Transform(AnimClass->GetStateMachineNodeProperties(), StateMachineNodeProperties, MakePropertyPath);
	ResolvedStateMachineNodeProperties = AnimClass->GetStateMachineNodeProperties();
	Algo::Transform(AnimClass->GetInitializationNodeProperties(), InitializationNodeProperties, MakePropertyPath);
	ResolvedInitializationNodeProperties = AnimClass->GetInitializationNodeProperties();

	SyncGroupNames = AnimClass->GetSyncGroupNames();
	GraphNameAssetPlayers = AnimClass->GetGraphAssetPlayerInformation();
	GraphBlendOptions = AnimClass->GetGraphBlendOptions();
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif // WITH_EDITOR

TArrayView<const FAnimNodeData> UAnimClassData::GetNodeData() const
{
	TArray<FAnimNodeData> Dummy;
	return Dummy;
}
