// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNode_ControlRig_Library.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimNode_ControlRig_Library)

FControlRigReference UAnimNodeControlRigLibrary::ConvertToControlRig(const FAnimNodeReference& Node, EAnimNodeReferenceConversionResult& Result)
{
	return FAnimNodeReference::ConvertToType<FControlRigReference>(Node, Result);
}

FControlRigReference UAnimNodeControlRigLibrary::SetControlRigClass(const FControlRigReference& Node, TSubclassOf<UControlRig> ControlRigClass)
{
	Node.CallAnimNodeFunction<FAnimNode_ControlRig>(
	TEXT("SetSequence"),
	[ControlRigClass](FAnimNode_ControlRig& InControlRigNode)
	{
		InControlRigNode.SetControlRigClass(ControlRigClass);
	});

	return Node;
}