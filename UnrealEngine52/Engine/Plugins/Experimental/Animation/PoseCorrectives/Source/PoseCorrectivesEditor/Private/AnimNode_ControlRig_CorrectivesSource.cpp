// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNode_ControlRig_CorrectivesSource.h"

void FAnimNode_ControlRig_CorrectivesSource::LinkNode(FAnimNode_Base* LinkNode)
{
	Source.SetLinkNode(LinkNode);
}

void FAnimNode_ControlRig_CorrectivesSource::SetControlRigUpdatePose(bool bSetControlRigUpdate)
{
	OutputSettings.bUpdatePose = bSetControlRigUpdate;
	OutputSettings.bUpdateCurves = bSetControlRigUpdate;
}