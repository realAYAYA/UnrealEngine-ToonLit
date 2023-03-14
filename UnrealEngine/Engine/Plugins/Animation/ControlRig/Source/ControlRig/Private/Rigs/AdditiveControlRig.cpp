// Copyright Epic Games, Inc. All Rights Reserved.

#include "Rigs/AdditiveControlRig.h"
#include "Animation/SmartName.h"
#include "Engine/SkeletalMesh.h"
#include "IControlRigObjectBinding.h"
#include "Components/SkeletalMeshComponent.h"
#include "Units/Execution/RigUnit_PrepareForExecution.h"
#include "Rigs/RigHierarchyController.h"
#include "Units/Execution/RigUnit_BeginExecution.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AdditiveControlRig)

#define LOCTEXT_NAMESPACE "AdditiveControlRig"

UAdditiveControlRig::UAdditiveControlRig(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCopyHierarchyBeforeConstruction = false;
	bResetInitialTransformsBeforeConstruction = false;
}

FName UAdditiveControlRig::GetControlName(const FName& InBoneName)
{
	if (InBoneName != NAME_None)
	{
		return FName(*(InBoneName.ToString() + TEXT("_CONTROL")));
	}

	// if bone name is coming as none, we don't append
	return NAME_None;
}

FName UAdditiveControlRig::GetNullName(const FName& InBoneName)
{
	if (InBoneName != NAME_None)
	{
		return FName(*(InBoneName.ToString() + TEXT("_NULL")));
	}

	// if bone name is coming as none, we don't append
	return NAME_None;
}

bool UAdditiveControlRig::ExecuteUnits(FRigUnitContext& InOutContext, const FName& InEventName)
{
	if (InEventName == FRigUnit_BeginExecution::EventName)
	{
		for (FRigUnit_AddBoneTransform& Unit : AddBoneRigUnits)
		{
			FName ControlName = GetControlName(Unit.Bone);
			const int32 Index = GetHierarchy()->GetIndex(FRigElementKey(ControlName, ERigElementType::Control));
			Unit.Transform = GetHierarchy()->GetLocalTransform(Index);
			Unit.ExecuteContext.Hierarchy = GetHierarchy();
			Unit.ExecuteContext.EventName = InEventName;
			Unit.Execute(InOutContext);
		}
	}
	else if(InEventName == FRigUnit_PrepareForExecution::EventName)
	{
		if (USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(GetObjectBinding()->GetBoundObject()))
		{
			CreateRigElements(SkeletalMeshComponent->GetSkeletalMeshAsset());
		}

		// add units and initialize
		AddBoneRigUnits.Reset();

		GetHierarchy()->ForEach<FRigBoneElement>([&](FRigBoneElement* BoneElement) -> bool
		{
			FRigUnit_AddBoneTransform NewUnit;
			NewUnit.Bone = BoneElement->GetName();
			NewUnit.bPropagateToChildren = true;
			AddBoneRigUnits.Add(NewUnit);
			return true;
		});
	}

	return true;
}

void UAdditiveControlRig::Initialize(bool bInitRigUnits /*= true*/)
{
	PostInitInstanceIfRequired();
	
	Super::Initialize(bInitRigUnits);

	if (GetObjectBinding() == nullptr)
	{
		return;
	}

	Execute(EControlRigState::Init, FRigUnit_BeginExecution::EventName);
	FRigUnitContext DefaultContext;
	ExecuteUnits(DefaultContext, FRigUnit_PrepareForExecution::EventName);
}

void UAdditiveControlRig::CreateRigElements(const FReferenceSkeleton& InReferenceSkeleton, const FSmartNameMapping* InSmartNameMapping)
{
	PostInitInstanceIfRequired();
	
	GetHierarchy()->Reset();
	if (URigHierarchyController* Controller = GetHierarchy()->GetController(true))
	{
		Controller->ImportBones(InReferenceSkeleton, NAME_None, false, false, true, false);

		if (InSmartNameMapping)
		{
			TArray<FName> NameArray;
			InSmartNameMapping->FillNameArray(NameArray);
			for (int32 Index = 0; Index < NameArray.Num(); ++Index)
			{
				 Controller->AddCurve(NameArray[Index], 0.f, false);
			}
		}

		// add control for all bone hierarchy
		GetHierarchy()->ForEach<FRigBoneElement>([&](FRigBoneElement* BoneElement) -> bool
        {
            const FName BoneName = BoneElement->GetName();
            const int32 ParentIndex = GetHierarchy()->GetFirstParent(BoneElement->GetIndex());
            const FName NullName = GetNullName(BoneName);// name conflict?
            const FName ControlName = GetControlName(BoneName); // name conflict?

            FRigElementKey NullKey;
		
            if (ParentIndex != INDEX_NONE)
            {
                FTransform GlobalTransform = GetHierarchy()->GetGlobalTransform(BoneElement->GetIndex());
                FTransform ParentTransform = GetHierarchy()->GetGlobalTransform(ParentIndex);
                FTransform LocalTransform = GlobalTransform.GetRelativeTransform(ParentTransform);
                NullKey = Controller->AddNull(NullName, GetHierarchy()->GetKey(ParentIndex), LocalTransform, false, false);
            }
            else
            {
                FTransform GlobalTransform = GetHierarchy()->GetGlobalTransform(BoneElement->GetIndex());
                NullKey = Controller->AddNull(NullName, FRigElementKey(), GlobalTransform, true, false);
            }

            FRigControlSettings Settings;
            Settings.DisplayName = BoneName;
			Settings.ControlType = ERigControlType::Transform;
            Controller->AddControl(ControlName, NullKey, Settings, FRigControlValue::Make(FTransform::Identity), FTransform::Identity, FTransform::Identity, false);

            return true;
        });
	}
}

void UAdditiveControlRig::CreateRigElements(const USkeletalMesh* InReferenceMesh)
{
	if (InReferenceMesh)
	{
		const USkeleton* Skeleton = InReferenceMesh->GetSkeleton();
		CreateRigElements(InReferenceMesh->GetRefSkeleton(), (Skeleton) ? Skeleton->GetSmartNameContainer(USkeleton::AnimCurveMappingName) : nullptr);
	}
}

#undef LOCTEXT_NAMESPACE



