// Copyright Epic Games, Inc. All Rights Reserved.
#include "Tools/ControlRigPose.h"
#include "Tools/ControlRigPoseProjectSettings.h"
#include "IControlRigObjectBinding.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ControlRigPose)

#if WITH_EDITOR
#include "ScopedTransaction.h"
#endif

#define LOCTEXT_NAMESPACE "ControlRigPose"

void FControlRigControlPose::SavePose(UControlRig* ControlRig, bool bUseAll)
{
	TArray<FRigControlElement*> CurrentControls;
	ControlRig->GetControlsInOrder(CurrentControls);
	CopyOfControls.SetNum(0);
	
	for (FRigControlElement* ControlElement : CurrentControls)
	{
		if (ControlRig->GetHierarchy()->IsAnimatable(ControlElement) && (bUseAll || ControlRig->IsControlSelected(ControlElement->GetName())))
		{
			//we store poses in default parent space so if not in that space we need to compensate it
			bool bHasNonDefaultParent = false;
			FTransform GlobalTransform;
			FRigElementKey SpaceKey = ControlRig->GetHierarchy()->GetActiveParent(ControlElement->GetKey());
			if (SpaceKey != ControlRig->GetHierarchy()->GetDefaultParentKey())
			{
				bHasNonDefaultParent = true;
				//to compensate we get the global, switch space, then reset global in that new space
				GlobalTransform = ControlRig->GetHierarchy()->GetGlobalTransform(ControlElement->GetKey());
				ControlRig->GetHierarchy()->SwitchToDefaultParent(ControlElement->GetKey());
				ControlRig->GetHierarchy()->SetGlobalTransform(ControlElement->GetKey(),GlobalTransform);

				ControlRig->Evaluate_AnyThread();
			}
			FRigControlCopy Copy(ControlElement, ControlRig->GetHierarchy());
			CopyOfControls.Add(Copy);
	
			if (bHasNonDefaultParent == true) 
			{
				ControlRig->GetHierarchy()->SwitchToParent(ControlElement->GetKey(),SpaceKey);
				ControlRig->GetHierarchy()->SetGlobalTransform(ControlElement->GetKey(), GlobalTransform);
			}
			
		}
	}
	SetUpControlMap();
}

void FControlRigControlPose::PastePose(UControlRig* ControlRig, bool bDoKey, bool bDoMirror)
{
	PastePoseInternal(ControlRig, bDoKey, bDoMirror, CopyOfControls);
	ControlRig->Evaluate_AnyThread();
	PastePoseInternal(ControlRig, bDoKey, bDoMirror, CopyOfControls);

}

void FControlRigControlPose::SetControlMirrorTransform(bool bDoLocal, UControlRig* ControlRig, const FName& Name, bool bIsMatched, 
	const FTransform& GlobalTransform, const FTransform& LocalTransform, bool bNotify, const  FRigControlModifiedContext&Context,bool bSetupUndo)
{
	if (bDoLocal || bIsMatched)
	{
		ControlRig->SetControlLocalTransform(Name, LocalTransform, bNotify,Context,bSetupUndo);

	}
	else
	{
		ControlRig->SetControlGlobalTransform(Name, GlobalTransform, bNotify,Context,bSetupUndo);
	}	
}

void FControlRigControlPose::PastePoseInternal(UControlRig* ControlRig, bool bDoKey, bool bDoMirror, const TArray<FRigControlCopy>& ControlsToPaste)
{
	FRigControlModifiedContext Context;
	Context.SetKey = bDoKey ? EControlRigSetKey::Always : EControlRigSetKey::DoNotCare;
	FControlRigPoseMirrorTable MirrorTable;
	if (bDoMirror)
	{
		MirrorTable.SetUpMirrorTable(ControlRig);
	}

	TArray<FRigControlElement*> SortedControls;
	ControlRig->GetControlsInOrder(SortedControls);
	
	const bool bDoLocal = true;
	const bool bSetupUndo = false;

	for (FRigControlElement* ControlElement : SortedControls)
	{
		if (!ControlRig->IsControlSelected(ControlElement->GetName()))
		{
			continue;
		}
		
		FRigControlCopy* CopyRigControl = MirrorTable.GetControl(*this, ControlElement->GetName());
		if (CopyRigControl)
		{
			//if not in default parent space we need to move it to default parent space first and then reset the global transforms
			bool bHasNonDefaultParent = false;
			FRigElementKey SpaceKey = ControlRig->GetHierarchy()->GetActiveParent(ControlElement->GetKey());
			if (SpaceKey != ControlRig->GetHierarchy()->GetDefaultParentKey())
			{
				bHasNonDefaultParent = true;
				ControlRig->GetHierarchy()->SwitchToDefaultParent(ControlElement->GetKey());
				ControlRig->Evaluate_AnyThread();
			}

			switch (ControlElement->Settings.ControlType)
			{
			case ERigControlType::Transform:
			case ERigControlType::TransformNoScale:
			case ERigControlType::EulerTransform:
			case ERigControlType::Position:
			case ERigControlType::Scale:
			case ERigControlType::Rotator:
			{
				if (bDoMirror == false)
				{
					if (bDoLocal) // -V547  
					{
						ControlRig->SetControlLocalTransform(ControlElement->GetName(), CopyRigControl->LocalTransform, true,Context,bSetupUndo);
					}
					else
					{
						ControlRig->SetControlGlobalTransform(ControlElement->GetName(), CopyRigControl->GlobalTransform, true,Context,bSetupUndo);
					}
				}
				else
				{
					FTransform GlobalTransform;
					FTransform LocalTransform;
					bool bIsMatched = MirrorTable.IsMatched(CopyRigControl->Name);
					MirrorTable.GetMirrorTransform(*CopyRigControl, bDoLocal,bIsMatched,GlobalTransform,LocalTransform);
					SetControlMirrorTransform(bDoLocal,ControlRig, ControlElement->GetName(), bIsMatched, GlobalTransform,LocalTransform,true,Context,bSetupUndo);
				}				
				break;
			}
			case ERigControlType::Float:
			{
				float Val = CopyRigControl->Value.Get<float>();
				ControlRig->SetControlValue<float>(ControlElement->GetName(), Val, true, Context,bSetupUndo);
				break;
			}
			case ERigControlType::Bool:
			{
				bool Val = CopyRigControl->Value.Get<bool>();
				ControlRig->SetControlValue<bool>(ControlElement->GetName(), Val, true, Context,bSetupUndo);
				break;
			}
			case ERigControlType::Integer:
			{
				int32 Val = CopyRigControl->Value.Get<int32>();
				ControlRig->SetControlValue<int32>(ControlElement->GetName(), Val, true, Context,bSetupUndo);
				break;
			}
			case ERigControlType::Vector2D:
			{
				FVector3f Val = CopyRigControl->Value.Get<FVector3f>();
				ControlRig->SetControlValue<FVector3f>(ControlElement->GetName(), Val, true, Context,bSetupUndo);
				break;
			}
			default:
				//TODO add log
				break;
			};

			if (bHasNonDefaultParent == true)
			{
				FTransform GlobalTransform = ControlRig->GetHierarchy()->GetGlobalTransform(ControlElement->GetKey());
				ControlRig->GetHierarchy()->SwitchToParent(ControlElement->GetKey(), SpaceKey);
				ControlRig->SetControlGlobalTransform(ControlElement->GetName(), GlobalTransform, true, Context, bSetupUndo);
			}
		}
	}
}

void FControlRigControlPose::BlendWithInitialPoses(FControlRigControlPose& InitialPose, UControlRig* ControlRig, bool bDoKey, bool bDoMirror, float BlendValue)
{
	if (InitialPose.CopyOfControls.Num() == 0)
	{
		return;
	}

	//though can be n^2 should be okay, we search from current Index which in most cases will be the same
	//not run often anyway
	FRigControlModifiedContext Context;
	Context.SetKey = bDoKey ? EControlRigSetKey::Always : EControlRigSetKey::DoNotCare;
	FControlRigPoseMirrorTable MirrorTable;
	if (bDoMirror)
	{
		MirrorTable.SetUpMirrorTable(ControlRig);
	}

	TArray<FRigControlElement*> SortedControls;
	ControlRig->GetControlsInOrder(SortedControls);
	
	const bool bDoLocal = true;
	const bool bSetupUndo = false;
	for (FRigControlElement* ControlElement : SortedControls)
	{
		if (!ControlRig->IsControlSelected(ControlElement->GetName()))
		{
			continue;
		}
		FRigControlCopy* CopyRigControl = MirrorTable.GetControl(*this, ControlElement->GetName());
		if (CopyRigControl)
		{
			FRigControlCopy* InitialFound = nullptr;
			int32* Index = InitialPose.CopyOfControlsNameToIndex.Find(CopyRigControl->Name);
			if (Index)
			{
				InitialFound = &(InitialPose.CopyOfControls[*Index]);
			}
			if (InitialFound && InitialFound->ControlType == CopyRigControl->ControlType)
			{
				if ((CopyRigControl->ControlType == ERigControlType::Transform || CopyRigControl->ControlType == ERigControlType::EulerTransform ||
					CopyRigControl->ControlType == ERigControlType::TransformNoScale || CopyRigControl->ControlType == ERigControlType::Position ||
					CopyRigControl->ControlType == ERigControlType::Rotator || CopyRigControl->ControlType == ERigControlType::Scale
					))
				{
					//if not in default parent space we need to move it to default parent space first and then reset the global transforms
					bool bHasNonDefaultParent = false;
					FRigElementKey SpaceKey = ControlRig->GetHierarchy()->GetActiveParent(ControlElement->GetKey());
					if (SpaceKey != ControlRig->GetHierarchy()->GetDefaultParentKey())
					{
						bHasNonDefaultParent = true;
						ControlRig->GetHierarchy()->SwitchToDefaultParent(ControlElement->GetKey());
						ControlRig->Evaluate_AnyThread();
					}
					if (bDoMirror == false)
					{
						if (bDoLocal == true)    // -V547  
						{
							FTransform Val = CopyRigControl->LocalTransform;
							FTransform InitialVal = InitialFound->LocalTransform;
							FVector Translation, Scale;
							FQuat Rotation;
							Translation = FMath::Lerp(InitialVal.GetTranslation(), Val.GetTranslation(), BlendValue);
							Rotation = FQuat::Slerp(InitialVal.GetRotation(), Val.GetRotation(), BlendValue); //doing slerp here not fast lerp, can be slow this is for content creation
							Scale = FMath::Lerp(InitialVal.GetScale3D(), Val.GetScale3D(), BlendValue);
							Val = FTransform(Rotation, Translation, Scale);
							ControlRig->SetControlLocalTransform(ControlElement->GetName(), Val, bDoKey,Context,bSetupUndo);
						}
						else
						{
							FTransform Val = CopyRigControl->GlobalTransform;
							FTransform InitialVal = InitialFound->GlobalTransform;
							FVector Translation, Scale;
							FQuat Rotation;
							Translation = FMath::Lerp(InitialVal.GetTranslation(), Val.GetTranslation(), BlendValue);
							Rotation = FQuat::Slerp(InitialVal.GetRotation(), Val.GetRotation(), BlendValue); //doing slerp here not fast lerp, can be slow this is for content creation
							Scale = FMath::Lerp(InitialVal.GetScale3D(), Val.GetScale3D(), BlendValue);
							Val = FTransform(Rotation, Translation, Scale);
							ControlRig->SetControlGlobalTransform(ControlElement->GetName(), Val, bDoKey,Context,bSetupUndo);
						}
					}
					else
					{
						FTransform GlobalTransform;
						FTransform LocalTransform;
						bool bIsMatched = MirrorTable.IsMatched(CopyRigControl->Name);
						MirrorTable.GetMirrorTransform(*CopyRigControl, bDoLocal, bIsMatched, GlobalTransform, LocalTransform);
						FVector InitialTranslation = InitialFound->GlobalTransform.GetTranslation();
						FQuat InitialGlobalRotation = InitialFound->GlobalTransform.GetRotation();
						FVector InitialGlobalScale = InitialFound->GlobalTransform.GetScale3D();

						FVector InitialLocalTranslation = InitialFound->LocalTransform.GetTranslation();
						FQuat InitialLocationRotation = InitialFound->LocalTransform.GetRotation();
						FVector InitialLocalScale = InitialFound->LocalTransform.GetScale3D();

						GlobalTransform.SetTranslation(FMath::Lerp(InitialTranslation, GlobalTransform.GetTranslation(), BlendValue));
						GlobalTransform.SetRotation(FQuat::Slerp(InitialGlobalRotation, GlobalTransform.GetRotation(), BlendValue)); //doing slerp here not fast lerp, can be slow this is for content creation
						GlobalTransform.SetScale3D(FMath::Lerp(InitialGlobalScale, GlobalTransform.GetScale3D(), BlendValue));

						LocalTransform.SetTranslation(FMath::Lerp(InitialLocalTranslation, LocalTransform.GetTranslation(), BlendValue));
						LocalTransform.SetRotation(FQuat::Slerp(InitialLocationRotation, LocalTransform.GetRotation(), BlendValue)); //doing slerp here not fast lerp, can be slow this is for content creation
						LocalTransform.SetScale3D(FMath::Lerp(InitialLocalScale, LocalTransform.GetScale3D(), BlendValue));

						SetControlMirrorTransform(bDoLocal,ControlRig, ControlElement->GetName(), bIsMatched, GlobalTransform,LocalTransform,bDoKey,Context,bSetupUndo);							
					}
					if (bHasNonDefaultParent == true)
					{
						FTransform GlobalTransform = ControlRig->GetHierarchy()->GetGlobalTransform(ControlElement->GetKey());
						ControlRig->GetHierarchy()->SwitchToParent(ControlElement->GetKey(), SpaceKey);
						ControlRig->SetControlGlobalTransform(ControlElement->GetName(), GlobalTransform, true, Context, bSetupUndo);
					}
				}
			}
		}
	}
}

bool FControlRigControlPose::ContainsName(const FName& Name) const
{
	const int32* Index = CopyOfControlsNameToIndex.Find(Name);
	return (Index && *Index >= 0);
}

void FControlRigControlPose::ReplaceControlName(const FName& Name, const FName& NewName)
{
	int32* Index = CopyOfControlsNameToIndex.Find(Name);
	if (Index && *Index >= 0)
	{
		FRigControlCopy& Control = CopyOfControls[*Index];
		Control.Name = NewName;
		CopyOfControlsNameToIndex.Remove(Name);
		CopyOfControlsNameToIndex.Add(Control.Name, *Index);
	}
}

TArray<FName> FControlRigControlPose::GetControlNames() const
{
	TArray<FName> Controls;
	Controls.Reserve(CopyOfControls.Num());
	for (const FRigControlCopy& Control : CopyOfControls)
	{
		Controls.Add(Control.Name);
	}
	return Controls;
}

void FControlRigControlPose::SetUpControlMap()
{
	CopyOfControlsNameToIndex.Reset();

	for (int32 Index = 0; Index < CopyOfControls.Num(); ++Index)
	{
		const FRigControlCopy& Control = CopyOfControls[Index];
		CopyOfControlsNameToIndex.Add(Control.Name, Index);
	}
}


UControlRigPoseAsset::UControlRigPoseAsset(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

void UControlRigPoseAsset::PostLoad()
{
	Super::PostLoad();
	Pose.SetUpControlMap();
}

void UControlRigPoseAsset::SavePose(UControlRig* InControlRig, bool bUseAll)
{
	Pose.SavePose(InControlRig,bUseAll);
}

void UControlRigPoseAsset::PastePose(UControlRig* InControlRig, bool bDoKey, bool bDoMirror)
{
#if WITH_EDITOR
	FScopedTransaction ScopedTransaction(LOCTEXT("PastePoseTransaction", "Paste Pose"));
	InControlRig->Modify();
#endif
	Pose.PastePose(InControlRig,bDoKey, bDoMirror);
}

void UControlRigPoseAsset::SelectControls(UControlRig* InControlRig, bool bDoMirror)
{
#if WITH_EDITOR
	FScopedTransaction ScopedTransaction(LOCTEXT("SelectControlTransaction", "Select Control"));
	InControlRig->Modify();
#endif
	InControlRig->ClearControlSelection();
	TArray<FName> Controls = Pose.GetControlNames();
	FControlRigPoseMirrorTable MirrorTable;
	FControlRigControlPose TempPose;
	if (bDoMirror)
	{
		MirrorTable.SetUpMirrorTable(InControlRig);
		TempPose.SavePose(InControlRig, true);
	}
	for (const FName& Name : Controls)
	{
		if (bDoMirror)
		{
			FRigControlCopy* CopyRigControl = MirrorTable.GetControl(TempPose, Name);
			if (CopyRigControl)
			{
				InControlRig->SelectControl(CopyRigControl->Name, true);
			}
			else
			{
				InControlRig->SelectControl(Name, true);
			}
		}
		else
		{
			InControlRig->SelectControl(Name, true);
		}
	}
}

void UControlRigPoseAsset::GetCurrentPose(UControlRig* InControlRig, FControlRigControlPose& OutPose)
{
	OutPose.SavePose(InControlRig, true);
}


TArray<FRigControlCopy> UControlRigPoseAsset::GetCurrentPose(UControlRig* InControlRig) 
{
	FControlRigControlPose TempPose;
	TempPose.SavePose(InControlRig,true);
	return TempPose.GetPoses();
}

void UControlRigPoseAsset::BlendWithInitialPoses(FControlRigControlPose& InitialPose, UControlRig* InControlRig, bool bDoKey, bool bDoMirror, float BlendValue)
{
	if (BlendValue > 0.0f)
	{
		Pose.BlendWithInitialPoses(InitialPose, InControlRig, bDoKey, bDoMirror, BlendValue);
	}
}

TArray<FName> UControlRigPoseAsset::GetControlNames() const
{
	return Pose.GetControlNames();
}

void UControlRigPoseAsset::ReplaceControlName(const FName& CurrentName, const FName& NewName)
{
	Pose.ReplaceControlName(CurrentName, NewName);
}

bool UControlRigPoseAsset::DoesMirrorMatch(UControlRig* ControlRig, const FName& ControlName) const
{
	FControlRigPoseMirrorTable MirrorTable;
	MirrorTable.SetUpMirrorTable(ControlRig);
	return (MirrorTable.IsMatched(ControlName));
}


#undef LOCTEXT_NAMESPACE


