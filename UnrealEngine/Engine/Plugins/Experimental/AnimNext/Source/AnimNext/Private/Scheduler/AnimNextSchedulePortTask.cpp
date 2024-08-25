// Copyright Epic Games, Inc. All Rights Reserved.

#include "Scheduler/AnimNextSchedulePortTask.h"
#include "Scheduler/AnimNextSchedulePort.h"
#include "ScheduleContext.h"
#include "Scheduler/ScheduleTermContext.h"
#include "AnimNextStats.h"

DEFINE_STAT(STAT_AnimNext_Task_Port);

void FAnimNextSchedulePortTask::RunPort(const UE::AnimNext::FScheduleContext& InScheduleContext) const
{
	SCOPE_CYCLE_COUNTER(STAT_AnimNext_Task_Port);

	using namespace UE::AnimNext;

	if(Port)
	{
		if(const UAnimNextSchedulePort* CDO = Port->GetDefaultObject<UAnimNextSchedulePort>())
		{
			FScheduleInstanceData& InstanceData = InScheduleContext.GetInstanceData();

			// Check and allocate remapped term layer
			FParamStackLayerHandle& TermLayerHandle = InstanceData.PortTermLayers[TaskIndex];
			if(!TermLayerHandle.IsValid())
			{
				TConstArrayView<FScheduleTerm> PortTerms = CDO->GetTerms();
				check(Terms.Num() == PortTerms.Num());

				TMap<FName, FName> Mapping;
				Mapping.Reserve(Terms.Num());
				for(int32 TermIndex = 0; TermIndex < Terms.Num(); ++TermIndex)
				{
					uint32 IntermediateTermIndex = Terms[TermIndex];
					const FPropertyBagPropertyDesc& PropertyDesc = InstanceData.IntermediatesData.GetPropertyBagStruct()->GetPropertyDescs()[IntermediateTermIndex];
					Mapping.Add(PropertyDesc.Name, PortTerms[TermIndex].GetName());
				}

				TermLayerHandle = FParamStack::MakeRemappedLayer(InstanceData.IntermediatesLayer, Mapping);
			}
			
			// Supply the I/O terms that the port needs
			FScheduleTermContext TermContext(InScheduleContext, InstanceData.PortTermLayers[TaskIndex]);
			CDO->Run(TermContext);
		}
	}
}