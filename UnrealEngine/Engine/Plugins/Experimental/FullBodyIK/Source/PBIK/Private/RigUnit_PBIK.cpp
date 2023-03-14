// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigUnit_PBIK.h"
#include "Units/RigUnitContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigUnit_PBIK)

FRigUnit_PBIK_Execute()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	LLM_SCOPE_BYNAME(TEXT("Animation/FBIK"));

	if (Context.State == EControlRigState::Init)
	{
		BoneSettingToSolverBoneIndex.Reset();
		Solver.Reset();
		SolverBoneToElementIndex.Reset();
		EffectorSolverIndices.Reset();
		bNeedsInit = true;
		return;
	}

	// only updates from here on...
	if (Context.State != EControlRigState::Update)
	{
		return;
	}

	URigHierarchy* Hierarchy = ExecuteContext.Hierarchy;
	if (Hierarchy == nullptr)
	{
		return;
	}

	if (bNeedsInit)
	{
		// check how many effectors are assigned to a bone
		int NumEffectors = 0;
		for (const FPBIKEffector& Effector : Effectors)
		{
			if (Hierarchy->GetIndex(FRigElementKey(Effector.Bone, ERigElementType::Bone)) != INDEX_NONE)
			{
				++NumEffectors; // bone is set and exists!
			}
		}

		// validate inputs are ready to be initialized
		bool bHasEffectors = NumEffectors > 0;
		bool bRootIsAssigned = Root != NAME_None;
		if (!(bHasEffectors && bRootIsAssigned))
		{
			return; // not setup yet
		}
		
		// create solver bone index to elements index map
		TArray<FRigBoneElement*> BoneElements = Hierarchy->GetBones(true);
		for (int B = 0; B < BoneElements.Num(); ++B)
		{
			SolverBoneToElementIndex.Add(BoneElements[B]->GetIndex());
		}

		// create bones
		for (int B = 0; B < BoneElements.Num(); ++B)
		{
			FName Name = BoneElements[B]->GetName();

			// get the first parent that is not excluded
			int32 ParentElementIndex = INDEX_NONE;
			FRigBaseElement* ParentElement = Hierarchy->GetFirstParent(BoneElements[B]);
			while (ParentElement)
			{
				if (!ExcludedBones.Contains(ParentElement->GetKey().Name))
				{
					ParentElementIndex = ParentElement->GetIndex();
					break;
				}
				ParentElement = Hierarchy->GetFirstParent(ParentElement);
			}

			// get the parent bone solver index
			int ParentIndex = -1;
			for (int P=0; P<SolverBoneToElementIndex.Num(); ++P)
			{
				if (SolverBoneToElementIndex[P] == ParentElementIndex)
				{
					ParentIndex = P;
					break;
				}
			}
			
			const FTransform OrigTransform = Hierarchy->GetTransform(BoneElements[B], ERigTransformType::InitialGlobal);
			const FVector InOrigPosition = OrigTransform.GetLocation();
			const FQuat InOrigRotation = OrigTransform.GetRotation();
			bool bIsRoot = BoneElements[B]->GetName() == Root;
			Solver.AddBone(Name, ParentIndex, InOrigPosition, InOrigRotation, bIsRoot);
		}
		
		// create effectors
		for (const FPBIKEffector& Effector : Effectors)
		{
			int32 IndexInSolver = Solver.AddEffector(Effector.Bone);
			EffectorSolverIndices.Add(IndexInSolver);
		}
		
		// initialize
		Solver.Initialize();
		bNeedsInit = false;
	}

	if (!Solver.IsReadyToSimulate())
	{
		return;
	}

	if (EffectorSolverIndices.Num() != Effectors.Num())
	{
		return;
	}

	// set bones to input pose
	for(int32 BoneIndex = 0; BoneIndex < Solver.GetNumBones(); BoneIndex++)
	{
		const FTransform GlobalTransform = Hierarchy->GetGlobalTransform(SolverBoneToElementIndex[BoneIndex]);
		Solver.SetBoneTransform(BoneIndex, GlobalTransform);
	}

	// invalidate the name lookup for the settings
	if(BoneSettingToSolverBoneIndex.Num() != BoneSettings.Num())
	{
		BoneSettingToSolverBoneIndex.Reset();
		while(BoneSettingToSolverBoneIndex.Num() < BoneSettings.Num())
		{
			BoneSettingToSolverBoneIndex.Add(INDEX_NONE);
		}
	}

	// update bone settings
	for (int32 BoneSettingIndex = 0; BoneSettingIndex < BoneSettings.Num(); BoneSettingIndex++)
	{
		const FPBIKBoneSetting& BoneSetting = BoneSettings[BoneSettingIndex];

		if(BoneSettingToSolverBoneIndex[BoneSettingIndex] == INDEX_NONE)
		{
			BoneSettingToSolverBoneIndex[BoneSettingIndex] = Solver.GetBoneIndex(BoneSetting.Bone);
			if(BoneSettingToSolverBoneIndex[BoneSettingIndex] == INDEX_NONE)
			{
				continue;
			}
		}

		int32 BoneIndex = BoneSettingToSolverBoneIndex[BoneSettingIndex];
		if (PBIK::FBoneSettings* InternalSettings = Solver.GetBoneSettings(BoneIndex))
		{
			BoneSetting.CopyToCoreStruct(*InternalSettings);
		}
	}

	// update effectors
	for (int E = 0; E < Effectors.Num(); ++E)
	{
		if (EffectorSolverIndices[E] == -1)
		{
			continue;
		}

		const FPBIKEffector& Effector = Effectors[E];
		FVector Position = Effector.Transform.GetLocation();
		FQuat Rotation = Effector.Transform.GetRotation();

		PBIK::FEffectorSettings EffectorSettings;
		EffectorSettings.PositionAlpha = Effector.PositionAlpha;
		EffectorSettings.RotationAlpha = Effector.RotationAlpha;
		EffectorSettings.StrengthAlpha = Effector.StrengthAlpha;
		EffectorSettings.PullChainAlpha = Effector.PullChainAlpha;
		EffectorSettings.PinRotation = Effector.PinRotation;
		
		Solver.SetEffectorGoal(EffectorSolverIndices[E], Position, Rotation, EffectorSettings);
	}

	// solve
	Solver.Solve(Settings);

	// copy transforms back
	const bool bPropagateTransform = false;
	for(int32 BoneIndex = 0; BoneIndex < Solver.GetNumBones(); BoneIndex++)
	{
		FTransform NewTransform;
		Solver.GetBoneGlobalTransform(BoneIndex, NewTransform);
		Hierarchy->SetGlobalTransform(SolverBoneToElementIndex[BoneIndex], NewTransform, false, bPropagateTransform);
	}

	// do all debug drawing
	Debug.Draw(Context.DrawInterface, &Solver);
}

