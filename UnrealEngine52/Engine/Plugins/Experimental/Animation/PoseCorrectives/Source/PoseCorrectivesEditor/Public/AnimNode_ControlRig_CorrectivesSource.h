// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimNode_ControlRig_ExternalSource.h"
#include "AnimNode_ControlRig_CorrectivesSource.generated.h"


USTRUCT()
struct FAnimNode_ControlRig_CorrectivesSource : public FAnimNode_ControlRig_ExternalSource
{
	GENERATED_BODY()

	void LinkNode(FAnimNode_Base* LinkNode);
	void SetControlRigUpdatePose(bool bSetControlRigUpdate);
};