// Copyright Epic Games, Inc. All Rights Reserved.

#include "Rigs/FKControlRig.h"
#include "Animation/SmartName.h"
#include "Engine/SkeletalMesh.h"
#include "IControlRigObjectBinding.h"
#include "Components/SkeletalMeshComponent.h"
#include "Units/Execution/RigUnit_PrepareForExecution.h"
#include "Settings/ControlRigSettings.h"
#include "Units/Execution/RigUnit_BeginExecution.h"
#include "Units/Execution/RigUnit_InverseExecution.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(FKControlRig)

#define LOCTEXT_NAMESPACE "OverrideControlRig"

UFKControlRig::UFKControlRig(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, ApplyMode(EControlRigFKRigExecuteMode::Replace)
	, CachedToggleApplyMode(EControlRigFKRigExecuteMode::Replace)
{
	bCopyHierarchyBeforeConstruction = false;
	bResetInitialTransformsBeforeConstruction = false;
}

FName UFKControlRig::GetControlName(const FName& InName, const ERigElementType& InType)
{
	if (InName != NAME_None)
	{
		const FString& PostFix = [InType]()
		{
			if (InType == ERigElementType::Curve)
			{
				return TEXT("_CURVE");
			}

			return TEXT("");
		}();
		
		return FName(*(InName.ToString() + PostFix + TEXT("_CONTROL")));
	}

	// if control name is coming as none, we don't append the postfix
	return NAME_None;
}

FName UFKControlRig::GetControlTargetName(const FName& InName, const ERigElementType& InType)
{
	if (InName != NAME_None)
	{
		const FString& PostFix = [InType]()
		{
			if (InType == ERigElementType::Curve)
			{
				return TEXT("_CURVE_CONTROL");
			}

			return TEXT("_CONTROL");
		}();

		FString StringName = InName.ToString();
		if (StringName.EndsWith(PostFix, ESearchCase::CaseSensitive))
		{
			StringName.RemoveFromEnd(PostFix);
			return FName(*StringName);
		}		
	}

	// If incoming name is NONE, or not correctly formatted according to expected post-fix return NAME_None
	return NAME_None;
}

bool UFKControlRig::ExecuteUnits(FRigUnitContext& InOutContext, const FName& InEventName)
{
	if (InOutContext.State != EControlRigState::Update)
	{
		// we don't need any initialization so
		// it's fine to consider this done here and return true.
		return true;
	}

#if WITH_EDITOR
	if(URigHierarchy* Hierarchy = GetHierarchy())
	{
		if(Hierarchy->IsTracingChanges())
		{
			Hierarchy->StorePoseForTrace(TEXT("UFKControlRig::BeginExecuteUnits"));
		}
	}
#endif	

	if (InEventName == FRigUnit_BeginExecution::EventName)
	{
		GetHierarchy()->ForEach<FRigBoneElement>([&](FRigBoneElement* BoneElement) -> bool
        {
			const FName ControlName = GetControlName(BoneElement->GetName(), BoneElement->GetType());
			const FRigElementKey ControlKey(ControlName, ERigElementType::Control);
			const int32 ControlIndex = GetHierarchy()->GetIndex(ControlKey);
			if (ControlIndex != INDEX_NONE && IsControlActive[ControlIndex])
			{
				FRigControlElement* Control = GetHierarchy()->Get<FRigControlElement>(ControlIndex);
				const FTransform LocalTransform = GetHierarchy()->GetLocalTransform(ControlIndex);
				switch (ApplyMode)
				{
					case EControlRigFKRigExecuteMode::Replace:
					{
						const FTransform OffsetTransform = GetHierarchy()->GetControlOffsetTransform(Control, ERigTransformType::InitialLocal);
						FTransform Transform = LocalTransform * OffsetTransform;
						Transform.NormalizeRotation();
						GetHierarchy()->SetTransform(BoneElement, Transform, ERigTransformType::CurrentLocal, true, false);
						break;
					}
					case EControlRigFKRigExecuteMode::Additive:
					{
						const FTransform PreviousTransform = GetHierarchy()->GetTransform(BoneElement, ERigTransformType::CurrentLocal);
						FTransform Transform = LocalTransform * PreviousTransform;
						Transform.NormalizeRotation();
						GetHierarchy()->SetTransform(BoneElement, Transform, ERigTransformType::CurrentLocal, true, false);
						break;
					}
					case EControlRigFKRigExecuteMode::Direct:
					{
						GetHierarchy()->SetTransform(BoneElement, LocalTransform, ERigTransformType::CurrentLocal, true, false);
						break;
					}
				}
			}
			return true;
		});

		GetHierarchy()->ForEach<FRigCurveElement>([&](FRigCurveElement* CurveElement) -> bool
        {
			const FName ControlName = GetControlName(CurveElement->GetName(), CurveElement->GetType());
			const FRigElementKey ControlKey(ControlName, ERigElementType::Control);
			const int32 ControlIndex = GetHierarchy()->GetIndex(ControlKey);
			
			if (ControlIndex != INDEX_NONE && IsControlActive[ControlIndex])
			{
				const float CurveValue = GetHierarchy()->GetControlValue(ControlIndex).Get<float>();

				switch (ApplyMode)
				{
					case EControlRigFKRigExecuteMode::Replace:
					case EControlRigFKRigExecuteMode::Direct:
					{
						GetHierarchy()->SetCurveValue(CurveElement, CurveValue, false /*bSetupUndo*/);
						break;
					}
					case EControlRigFKRigExecuteMode::Additive:
					{
						const float PreviousValue = GetHierarchy()->GetCurveValue(CurveElement);
						GetHierarchy()->SetCurveValue(CurveElement, PreviousValue + CurveValue, false /*bSetupUndo*/);
						break;
					}
				}
			}
			return true;
		});
	}
	else if (InEventName == FRigUnit_InverseExecution::EventName)
	{
		const bool bNotify = true;
		const FRigControlModifiedContext Context = FRigControlModifiedContext();
		const bool bSetupUndo = false;

		GetHierarchy()->Traverse([&](FRigBaseElement* InElement, bool& bContinue)
		{
			if(!InElement->IsA<FRigBoneElement>())
			{
				bContinue = false;
				return;
			}

			FRigBoneElement* BoneElement = CastChecked<FRigBoneElement>(InElement);

			const FName ControlName = GetControlName(BoneElement->GetName(), BoneElement->GetType());
			const FRigElementKey ControlKey(ControlName, ERigElementType::Control);
			const int32 ControlIndex = GetHierarchy()->GetIndex(ControlKey);
			
			switch (ApplyMode)
			{
				case EControlRigFKRigExecuteMode::Replace:
				case EControlRigFKRigExecuteMode::Additive:
				{
					// during inversion we assume Replace Mode
					FRigControlElement* Control = GetHierarchy()->GetChecked<FRigControlElement>(ControlIndex);
					const FTransform Offset = GetHierarchy()->GetControlOffsetTransform(Control, ERigTransformType::InitialLocal);
					const FTransform Current = GetHierarchy()->GetTransform(BoneElement, ERigTransformType::CurrentLocal);
			
					FTransform Transform = Current.GetRelativeTransform(Offset);
					Transform.NormalizeRotation();

					SetControlValue(ControlName, FRigControlValue::Make(FEulerTransform(Transform)), bNotify, Context, bSetupUndo);
					break;
				}

				case EControlRigFKRigExecuteMode::Direct:
				{
					FTransform Transform = GetHierarchy()->GetTransform(BoneElement, ERigTransformType::CurrentLocal);
					Transform.NormalizeRotation();
					SetControlValue(ControlName, FRigControlValue::Make(FEulerTransform(Transform)), bNotify, Context, bSetupUndo);

					break;
				}
			}

		}, true);

		GetHierarchy()->ForEach<FRigCurveElement>([&](FRigCurveElement* CurveElement) -> bool
        {
			const FName ControlName = GetControlName(CurveElement->GetName(), CurveElement->GetType());
			const FRigElementKey ControlKey(ControlName, ERigElementType::Control);
			const int32 ControlIndex = GetHierarchy()->GetIndex(ControlKey);

			const float CurveValue = GetHierarchy()->GetCurveValue(CurveElement);
			SetControlValue(ControlName, FRigControlValue::Make(CurveValue), bNotify, Context, bSetupUndo);

			return true;
		});
	}

#if WITH_EDITOR
	if(URigHierarchy* Hierarchy = GetHierarchy())
	{
		if(Hierarchy->IsTracingChanges())
		{
			Hierarchy->StorePoseForTrace(TEXT("UFKControlRig::EndExecuteUnits"));
			Hierarchy->DumpTransformStackToFile();
		}
	}
#endif
	
	return true;
}

void UFKControlRig::SetBoneInitialTransformsFromSkeletalMeshComponent(USkeletalMeshComponent* InSkelMeshComp,
	bool bUseAnimInstance)
{
	Super::SetBoneInitialTransformsFromSkeletalMeshComponent(InSkelMeshComp, bUseAnimInstance);

#if WITH_EDITOR
	if(URigHierarchy* Hierarchy = GetHierarchy())
	{
		if(Hierarchy->IsTracingChanges())
		{
			Hierarchy->StorePoseForTrace(TEXT("UFKControlRig::SetBoneInitialTransformsFromSkeletalMeshComponent"));
		}
	}
#endif	

	SetControlOffsetsFromBoneInitials();
}

void UFKControlRig::Initialize(bool bInitRigUnits /*= true*/)
{
	PostInitInstanceIfRequired();

	Super::Initialize(bInitRigUnits);

	if (GetObjectBinding() == nullptr)
	{
		return;
	}

	// we do this after Initialize because Initialize will copy from CDO. 
	// create hierarchy from the incoming skeleton
	if (USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(GetObjectBinding()->GetBoundObject()))
	{
		CreateRigElements(SkeletalMeshComponent->GetSkeletalMeshAsset());
	}
	else if (USkeleton* Skeleton = Cast<USkeleton>(GetObjectBinding()->GetBoundObject()))
	{
		const FReferenceSkeleton& RefSkeleton = Skeleton->GetReferenceSkeleton();
		CreateRigElements(RefSkeleton, (Skeleton) ? Skeleton->GetSmartNameContainer(USkeleton::AnimCurveMappingName) : nullptr);
	}

	// execute init
	Execute(EControlRigState::Init, FRigUnit_BeginExecution::EventName);
}

TArray<FName> UFKControlRig::GetControlNames()
{
	TArray<FRigControlElement*> Controls;
	GetControlsInOrder(Controls);

	TArray<FName> Names;
	for (FRigControlElement* ControlElement : Controls) 
	{
		Names.Add(ControlElement->GetName());
	}
	return Names;
}

bool UFKControlRig::GetControlActive(int32 Index) const
{
	if (Index >= 0 && Index < IsControlActive.Num())
	{
		return IsControlActive[Index];
	}
	return false;
}

void UFKControlRig::SetControlActive(int32 Index, bool bActive)
{
	if (Index >= 0 && Index < IsControlActive.Num())
	{
		IsControlActive[Index] = bActive;
	}
}

void UFKControlRig::SetControlActive(const TArray<FFKBoneCheckInfo>& BoneChecks)
{
	for (const FFKBoneCheckInfo& Info : BoneChecks)
	{
		SetControlActive(Info.BoneID, Info.bActive);
	}
}

void UFKControlRig::CreateRigElements(const FReferenceSkeleton& InReferenceSkeleton, const FSmartNameMapping* InSmartNameMapping)
{
	PostInitInstanceIfRequired();

	GetHierarchy()->Reset();
	if (URigHierarchyController* Controller = GetHierarchy()->GetController(true))
	{
		Controller->ImportBones(InReferenceSkeleton, NAME_None, false, false, true, false);

		if (InitializationOptions.bGenerateCurveControls)
		{
			// try to add curves for manually specified names
			if (InitializationOptions.CurveNames.Num() > 0)
            {
            	for (int32 Index = 0; Index < InitializationOptions.CurveNames.Num(); ++Index)
            	{
            		if(InSmartNameMapping && InSmartNameMapping->FindUID(InitializationOptions.CurveNames[Index]) != SmartName::MaxUID)
            		{
            			Controller->AddCurve(InitializationOptions.CurveNames[Index], 0.f, false);
            		}
            	}
            }
			// add all curves found on the skeleton
            else if (InSmartNameMapping && InitializationOptions.bImportCurves)
            {
            	TArray<FName> NameArray;
            	InSmartNameMapping->FillNameArray(NameArray);
            	for (int32 Index = 0; Index < NameArray.Num(); ++Index)
            	{
            		Controller->AddCurve(NameArray[Index], 0.f, false);
            	}
            }
		}		

		// add control for all bone hierarchy 
		if (InitializationOptions.bGenerateBoneControls)
		{
			auto CreateControlForBoneElement = [this, Controller](const FRigBoneElement* BoneElement)
			{
				const FName BoneName = BoneElement->GetName();
				const FName ControlName = GetControlName(BoneName, BoneElement->GetType());
				const FRigElementKey ParentKey = GetHierarchy()->GetFirstParent(BoneElement->GetKey());

				FRigControlSettings Settings;
				Settings.ControlType = ERigControlType::EulerTransform;
				Settings.DisplayName = BoneName;
					
				Controller->AddControl(ControlName, ParentKey, Settings, FRigControlValue::Make(FEulerTransform::Identity), FTransform::Identity, FTransform::Identity, false);
			};
			
			if (InitializationOptions.BoneNames.Num())
			{
				for (const FName& BoneName : InitializationOptions.BoneNames)
				{
					FRigElementKey BoneElementKey(BoneName, ERigElementType::Bone);
					if (const FRigBoneElement* BoneElement = GetHierarchy()->Find<FRigBoneElement>(BoneElementKey))
					{
						CreateControlForBoneElement(BoneElement);
					}
				}
			}
			else
			{
				GetHierarchy()->ForEach<FRigBoneElement>([&](const FRigBoneElement* BoneElement) -> bool
				{
					CreateControlForBoneElement(BoneElement);
					return true;
				});
			}

			SetControlOffsetsFromBoneInitials();
		}
		
		if (InitializationOptions.bGenerateCurveControls)
		{
			GetHierarchy()->ForEach<FRigCurveElement>([&](const FRigCurveElement* CurveElement) -> bool
			{
				const FName ControlName = GetControlName(CurveElement->GetName(), CurveElement->GetType());

				FRigControlSettings Settings;
				Settings.ControlType = ERigControlType::Float;
				Settings.DisplayName = CurveElement->GetName();

				const FName DisplayCurveControlName(*(CurveElement->GetName().ToString() + TEXT(" Curve")));
				Settings.DisplayName = DisplayCurveControlName;
				Controller->AddControl(ControlName, FRigElementKey(), Settings, FRigControlValue::Make(CurveElement->Value), FTransform::Identity, FTransform::Identity, false);
				
				return true;
			});
		}

		RefreshActiveControls();
	}
}

void UFKControlRig::SetControlOffsetsFromBoneInitials()
{
	GetHierarchy()->Traverse([&](FRigBaseElement* InElement, bool& bContinue)
	{
		if(!InElement->IsA<FRigBoneElement>())
		{
			bContinue = false;
			return;
		}

		FRigBoneElement* BoneElement = CastChecked<FRigBoneElement>(InElement);
		const FName BoneName = BoneElement->GetName();
		const FName ControlName = GetControlName(BoneName, BoneElement->GetType());

		FRigControlElement* ControlElement = GetHierarchy()->Find<FRigControlElement>(FRigElementKey(ControlName, ERigElementType::Control));
		if(ControlElement == nullptr)
		{
			return;
		}
			
		const FRigElementKey ParentKey = GetHierarchy()->GetFirstParent(BoneElement->GetKey());

		FTransform OffsetTransform;
		if (ParentKey.IsValid())
		{
			FTransform GlobalTransform = GetHierarchy()->GetGlobalTransformByIndex(BoneElement->GetIndex(), true);
			FTransform ParentTransform = GetHierarchy()->GetGlobalTransform(ParentKey, true);
			OffsetTransform = GlobalTransform.GetRelativeTransform(ParentTransform);
		}
		else
		{
			OffsetTransform = GetHierarchy()->GetLocalTransformByIndex(BoneElement->GetIndex(), true);
		}

		OffsetTransform.NormalizeRotation();

		GetHierarchy()->SetControlOffsetTransform(ControlElement, OffsetTransform, ERigTransformType::InitialLocal, false, false, true);

	}, true);
}

void UFKControlRig::RefreshActiveControls()
{
	if (IsControlActive.Num() != GetHierarchy()->Num())
	{
		IsControlActive.Empty(GetHierarchy()->Num());
		IsControlActive.SetNum(GetHierarchy()->Num());
		for (bool& bIsActive : IsControlActive)
		{
			bIsActive = true;
		}
	}
}

void UFKControlRig::CreateRigElements(const USkeletalMesh* InReferenceMesh)
{
	if (InReferenceMesh)
	{
		const USkeleton* Skeleton = InReferenceMesh->GetSkeleton();
		CreateRigElements(InReferenceMesh->GetRefSkeleton(), (Skeleton) ? Skeleton->GetSmartNameContainer(USkeleton::AnimCurveMappingName) : nullptr);
	}
}

void UFKControlRig::SetApplyMode(EControlRigFKRigExecuteMode InMode)
{
	ApplyMode = InMode;
}

void UFKControlRig::ToggleApplyMode()
{
	if (ApplyMode == EControlRigFKRigExecuteMode::Additive)
	{
		ApplyMode = CachedToggleApplyMode;
	}
	else
	{
		CachedToggleApplyMode = ApplyMode;
		ApplyMode = EControlRigFKRigExecuteMode::Additive;
	}
	if (ApplyMode == EControlRigFKRigExecuteMode::Additive)
	{
		FRigControlModifiedContext Context;
		Context.SetKey = EControlRigSetKey::Never;
		const bool bSetupUndo = false;

		GetHierarchy()->ForEach<FRigControlElement>([&](FRigControlElement* ControlElement) -> bool
        {
            if (ControlElement->Settings.ControlType == ERigControlType::EulerTransform)
			{
				SetControlValue<FEulerTransform>(ControlElement->GetName(), FEulerTransform::Identity, true, Context, bSetupUndo);
			}
			else if (ControlElement->Settings.ControlType == ERigControlType::Float)
			{
				SetControlValue<float>(ControlElement->GetName(), 0.f, true, Context, bSetupUndo);
			}

			return true;
		});
	}
	else
	{
		FRigControlModifiedContext Context;
		Context.SetKey = EControlRigSetKey::Never;
		const bool bSetupUndo = false;

		GetHierarchy()->ForEach<FRigControlElement>([&](FRigControlElement* ControlElement) -> bool
        {
            if (ControlElement->Settings.ControlType == ERigControlType::EulerTransform)
			{
				const FRigControlValue::FEulerTransform_Float InitValue =
					GetHierarchy()->GetControlValue(ControlElement, ERigControlValueType::Initial).Get<FRigControlValue::FEulerTransform_Float>();
				SetControlValue<FRigControlValue::FEulerTransform_Float>(ControlElement->GetName(), InitValue, true, Context, bSetupUndo);
			}
			else if (ControlElement->Settings.ControlType == ERigControlType::Float)
			{
				const float InitValue = GetHierarchy()->GetControlValue(ControlElement, ERigControlValueType::Initial).Get<float>();
				SetControlValue<float>(ControlElement->GetName(), InitValue, true, Context, bSetupUndo);
			}

			return true;
		});
	}
}

#undef LOCTEXT_NAMESPACE



