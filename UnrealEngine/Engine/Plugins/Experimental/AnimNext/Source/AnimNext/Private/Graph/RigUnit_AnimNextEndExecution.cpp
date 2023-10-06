// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/RigUnit_AnimNextEndExecution.h"
#include "Units/RigUnitContext.h"
#include "Param/ParamStack.h"
#include "Context.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigUnit_AnimNextEndExecution)

FRigUnit_AnimNextEndExecution_Bool_Execute()
{

}

FRigUnit_AnimNextEndExecution_Float_Execute()
{

}

FRigUnit_AnimNextEndExecution_LODPose_Execute()
{
	using namespace UE::AnimNext;

	const FContext& InterfaceContext = ExecuteContext.GetContext();
	FAnimNextGraphLODPose& ResultPose = InterfaceContext.GetMutableParamStack().GetMutableParam<FAnimNextGraphLODPose>("ResultPose");

	ResultPose = Result;
}