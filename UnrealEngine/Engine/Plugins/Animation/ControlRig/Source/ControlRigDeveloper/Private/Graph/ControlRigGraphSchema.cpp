// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/ControlRigGraphSchema.h"
#include "Graph/ControlRigGraph.h"
#include "Units/Execution/RigUnit_BeginExecution.h"
#include "Units/Execution/RigUnit_PrepareForExecution.h"
#include "Units/Execution/RigUnit_InverseExecution.h"
#include "Units/Execution/RigUnit_InteractionExecution.h"
#include "Units/Modules/RigUnit_ConnectorExecution.h"


#include UE_INLINE_GENERATED_CPP_BY_NAME(ControlRigGraphSchema)



#define LOCTEXT_NAMESPACE "ControlRigGraphSchema"

const FName UControlRigGraphSchema::GraphName_ControlRig(TEXT("Rig"));

UControlRigGraphSchema::UControlRigGraphSchema()
{
}

FLinearColor UControlRigGraphSchema::GetPinTypeColor(const FEdGraphPinType& PinType) const
{
	const FName& TypeName = PinType.PinCategory;
	if (TypeName == UEdGraphSchema_K2::PC_Struct)
	{
		if (UStruct* Struct = Cast<UStruct>(PinType.PinSubCategoryObject))
		{
			if (Struct == FRigElementKey::StaticStruct() || Struct == FRigElementKeyCollection::StaticStruct())
			{
				return FLinearColor(0.0, 0.6588, 0.9490);
			}

			if (Struct == FRigElementKey::StaticStruct() || Struct == FRigPose::StaticStruct())
			{
				return FLinearColor(0.0, 0.3588, 0.5490);
			}
		}
	}
	
	return Super::GetPinTypeColor(PinType);
}

bool UControlRigGraphSchema::IsRigVMDefaultEvent(const FName& InEventName) const
{
	if(Super::IsRigVMDefaultEvent(InEventName))
	{
		return true;
	}
	
	return InEventName == FRigUnit_BeginExecution::EventName ||
		InEventName == FRigUnit_PreBeginExecution::EventName ||
		InEventName == FRigUnit_PostBeginExecution::EventName ||
		InEventName == FRigUnit_InverseExecution::EventName ||
		InEventName == FRigUnit_PrepareForExecution::EventName ||
		InEventName == FRigUnit_InteractionExecution::EventName ||
		InEventName == FRigUnit_ConnectorExecution::EventName;
}

#undef LOCTEXT_NAMESPACE

