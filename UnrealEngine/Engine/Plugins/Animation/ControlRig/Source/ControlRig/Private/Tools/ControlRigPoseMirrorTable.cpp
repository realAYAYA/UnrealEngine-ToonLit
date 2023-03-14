// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tools/ControlRigPoseMirrorTable.h"
#include "Tools/ControlRigPoseMirrorSettings.h"
#include "ControlRig.h"
#include "Tools/ControlRigPose.h"

void FControlRigPoseMirrorTable::SetUpMirrorTable(const UControlRig* ControlRig)
{
	const UControlRigPoseMirrorSettings* Settings = GetDefault<UControlRigPoseMirrorSettings>();
	MatchedControls.Reset();
	if (Settings && ControlRig)
	{
		TArray<FRigControlElement*> CurrentControls = ControlRig->AvailableControls();
		for (FRigControlElement* ControlElement : CurrentControls)
		{
			FString CurrentString = ControlElement->GetName().ToString();
			if (CurrentString.Contains(Settings->RightSide))
			{
				FString NewString = CurrentString.Replace(*Settings->RightSide, *Settings->LeftSide);
				FName CurrentName(*CurrentString);
				FName NewName(*NewString);
				MatchedControls.Add(NewName, CurrentName);
			}
			else if (CurrentString.Contains(Settings->LeftSide))
			{
				FString NewString = CurrentString.Replace(*Settings->LeftSide, *Settings->RightSide);
				FName CurrentName(*CurrentString);
				FName NewName(*NewString);
				MatchedControls.Add(NewName, CurrentName);
			}
		}
	}
}

FRigControlCopy* FControlRigPoseMirrorTable::GetControl(FControlRigControlPose& Pose, FName Name)
{
	TArray<FRigControlCopy> CopyOfControls;
	
	if (MatchedControls.Num() > 0) 
	{
		if (const FName* MatchedName = MatchedControls.Find(Name))
		{
			int32* Index = Pose.CopyOfControlsNameToIndex.Find(*MatchedName);
			if (Index != nullptr && (*Index) >= 0 && (*Index) < Pose.CopyOfControls.Num())
			{
				return &(Pose.CopyOfControls[*Index]);
			}
		}
	}
	//okay not matched so just find it.
	int32* Index = Pose.CopyOfControlsNameToIndex.Find(Name);
	if (Index != nullptr && (*Index) >= 0 && (*Index) < Pose.CopyOfControls.Num())
	{
		return &(Pose.CopyOfControls[*Index]);
	}

	return nullptr;
}

bool FControlRigPoseMirrorTable::IsMatched(const FName& Name) const
{
	if (MatchedControls.Num() > 0)
	{
		if (const FName* MatchedName = MatchedControls.Find(Name))
		{
			return true;
		}
	}
	return false;
}

//Now returns mirrored global and local unmirrored
void FControlRigPoseMirrorTable::GetMirrorTransform(const FRigControlCopy& ControlCopy, bool bDoLocal, bool bIsMatched, FTransform& OutGlobalTransform,
	FTransform& OutLocalTransform) const
{
	const UControlRigPoseMirrorSettings* Settings = GetDefault<UControlRigPoseMirrorSettings>();
	FTransform GlobalTransform = ControlCopy.GlobalTransform;
	OutGlobalTransform = GlobalTransform;
	FTransform LocalTransform = ControlCopy.LocalTransform;
	OutLocalTransform = LocalTransform;
	if (Settings)
	{
		if (!bIsMatched  && (OutGlobalTransform.GetRotation().IsIdentity() == false || OutLocalTransform.GetRotation().IsIdentity() == false))
		{
			FRigMirrorSettings MirrorSettings;
			MirrorSettings.MirrorAxis = Settings->MirrorAxis;
			MirrorSettings.AxisToFlip = Settings->AxisToFlip;
			FTransform NewTransform = MirrorSettings.MirrorTransform(GlobalTransform);
			OutGlobalTransform.SetTranslation(NewTransform.GetTranslation());
			OutGlobalTransform.SetRotation(NewTransform.GetRotation());

			FTransform NewLocalTransform = MirrorSettings.MirrorTransform(LocalTransform);
			OutLocalTransform.SetTranslation(NewLocalTransform.GetTranslation());
			OutLocalTransform.SetRotation(NewLocalTransform.GetRotation());
			return;
		}
	}
}
