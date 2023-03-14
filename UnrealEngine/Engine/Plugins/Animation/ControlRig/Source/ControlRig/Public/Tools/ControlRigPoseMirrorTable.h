// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

/**
* Class to hold information on how a Pose May be Mirrored
*
*/

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"

class UControlRig;
struct FRigControl;
struct FRigControlCopy;
struct FControlRigControlPose;

struct FControlRigPoseMirrorTable
{
public:
	FControlRigPoseMirrorTable() {};
	~FControlRigPoseMirrorTable() {};

	/*Set up the Mirror Table*/
	void SetUpMirrorTable(const UControlRig* ControlRig);

	/*Get the matched control with the given name*/
	FRigControlCopy* GetControl(FControlRigControlPose& Pose, FName ControlrigName);

	/*Whether or not the Control with this name is matched*/
	bool IsMatched(const FName& ControlName) const;

	/*Return the Mirrored Global(In Control Rig Space) Translation and Mirrored Local Rotation*/
	void GetMirrorTransform(const FRigControlCopy& ControlCopy, bool bDoLocal, bool bIsMatched,FTransform& OutGlobalTransform, FTransform& OutLocalTransform) const;

private:

	TMap<FName, FName>  MatchedControls;

};