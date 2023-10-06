// Copyright Epic Games, Inc. All Rights Reserved.
#include "Tools/ControlRigPoseMirrorSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ControlRigPoseMirrorSettings)

UControlRigPoseMirrorSettings::UControlRigPoseMirrorSettings()
{
	RightSide = TEXT("_r_");
	LeftSide = TEXT("_l_");
	MirrorAxis = EAxis::X;
	AxisToFlip = EAxis::X;
}

