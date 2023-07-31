// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNode_ControlRig_ExternalSource.h"
#include "ControlRig.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimNode_ControlRig_ExternalSource)

FAnimNode_ControlRig_ExternalSource::FAnimNode_ControlRig_ExternalSource()
{
}

void FAnimNode_ControlRig_ExternalSource::SetControlRig(UControlRig* InControlRig)
{
	ControlRig = InControlRig;
	// requires initializing animation system
}

UControlRig* FAnimNode_ControlRig_ExternalSource::GetControlRig() const
{
	return (ControlRig.IsValid()? ControlRig.Get() : nullptr);
}

TSubclassOf<UControlRig> FAnimNode_ControlRig_ExternalSource::GetControlRigClass() const
{
	if(UControlRig* CR = GetControlRig())
	{
		return CR->GetClass();
	}
	return nullptr;
}


