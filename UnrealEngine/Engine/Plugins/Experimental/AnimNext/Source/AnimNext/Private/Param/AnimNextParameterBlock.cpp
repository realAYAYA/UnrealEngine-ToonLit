// Copyright Epic Games, Inc. All Rights Reserved.

#include "Param/AnimNextParameterBlock.h"
#include "RigVMCore/RigVMMemoryStorage.h"
#include "RigVMRuntimeDataRegistry.h"
#include "UObject/AssetRegistryTagsContext.h"
#include "UObject/ObjectSaveContext.h"
#include "UObject/Package.h"
#include "Graph/RigUnit_AnimNextBeginExecution.h"
#include "Param/AnimNextParameterExecuteContext.h"
#include "AnimNextStats.h"

DEFINE_STAT(STAT_AnimNext_ParamBlock_UpdateLayer);

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimNextParameterBlock)

UAnimNextParameterBlock::UAnimNextParameterBlock(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ExtendedExecuteContext.SetContextPublicDataStruct(FAnimNextParameterExecuteContext::StaticStruct());
}

void UAnimNextParameterBlock::UpdateLayer(UE::AnimNext::FParamStackLayerHandle& InHandle, float InDeltaTime) const
{
	SCOPE_CYCLE_COUNTER(STAT_AnimNext_ParamBlock_UpdateLayer);
	
	if (VM)
	{
		if (TSharedPtr<UE::AnimNext::FRigVMRuntimeData> RuntimeData = UE::AnimNext::FRigVMRuntimeDataRegistry::FindOrAddLocalRuntimeData(VM, GetRigVMExtendedExecuteContext()).Pin())
		{
			FRigVMExtendedExecuteContext& Context = RuntimeData->Context;

			check(Context.VMHash == VM->GetVMHash());

			FAnimNextParameterExecuteContext& AnimNextParameterContext = Context.GetPublicDataSafe<FAnimNextParameterExecuteContext>();

			// Param block setup
			AnimNextParameterContext.SetParamContextData(InHandle);

			// RigVM setup
			AnimNextParameterContext.SetDeltaTime(InDeltaTime);

			VM->ExecuteVM(Context, FRigUnit_AnimNextBeginExecution::EventName);
		}
	}
}
