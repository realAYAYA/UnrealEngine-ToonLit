// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigEditor/IKRigAutoFBIK.h"

#include "IKRigEditor.h"
#include "Rig/Solvers/IKRig_FBIKSolver.h"
#include "RigEditor/IKRigAutoCharacterizer.h"
#include "RigEditor/IKRigController.h"
#include "Templates/SubclassOf.h"

#define LOCTEXT_NAMESPACE "AutoFBIK"

void FAutoFBIKCreator::CreateFBIKSetup(const UIKRigController& IKRigController, FAutoFBIKResults& Results) const
{
	// ensure we have a mesh to operate on
	USkeletalMesh* Mesh = IKRigController.GetSkeletalMesh();
	if (!Mesh)
	{
		Results.Outcome = EAutoFBIKResult::MissingMesh;
		return;
	}

	// auto generate a retarget definition
	FAutoCharacterizeResults CharacterizeResults;
	IKRigController.AutoGenerateRetargetDefinition(CharacterizeResults);
	if (!CharacterizeResults.bUsedTemplate)
	{
		Results.Outcome = EAutoFBIKResult::UnknownSkeletonType;
		return;
	}
	
	// create all the goals in the template
	TArray<FName> GoalNames;
	const TArray<FBoneChain>& ExpectedChains = CharacterizeResults.AutoRetargetDefinition.RetargetDefinition.BoneChains;
	for (const FBoneChain& ExpectedChain : ExpectedChains)
	{
		if (ExpectedChain.IKGoalName == NAME_None)
		{
			continue;
		}
		
		const FBoneChain* Chain = IKRigController.GetRetargetChainByName(ExpectedChain.ChainName);
		FName GoalName = (Chain && Chain->IKGoalName != NAME_None) ? Chain->IKGoalName : ExpectedChain.IKGoalName;
		const UIKRigEffectorGoal* ChainGoal = IKRigController.GetGoal(GoalName);
		if (!ChainGoal)
		{
			// create new goal
			GoalName = IKRigController.AddNewGoal(GoalName, ExpectedChain.EndBone.BoneName);
			if (Chain)
			{
				IKRigController.SetRetargetChainGoal(Chain->ChainName, GoalName);
			}
			else
			{
				UE_LOG(LogIKRigEditor, Warning, TEXT("Auto FBIK created goal for a limb, but it did not have a retarget chain. %s"), *ExpectedChain.ChainName.ToString());
			}
		}

		GoalNames.Add(GoalName);
	}

	// create IK solver and attach all the goals to it
	const int32 SolverIndex = IKRigController.AddSolver(UIKRigFBIKSolver::StaticClass());
	for (const FName& GoalName : GoalNames)
	{
		IKRigController.ConnectGoalToSolver(GoalName, SolverIndex);	
	}

	// set the root of the solver
	const bool bSetRoot = IKRigController.SetRootBone(CharacterizeResults.AutoRetargetDefinition.RetargetDefinition.RootBone, SolverIndex);
	if (!bSetRoot)
	{
		Results.Outcome = EAutoFBIKResult::MissingRootBone;
		return;
	}

	// update solver settings for retargeting
	UIKRigFBIKSolver* Solver = CastChecked<UIKRigFBIKSolver>(IKRigController.GetSolverAtIndex(SolverIndex));
	// set the root behavior to "free", allows pelvis motion only when needed to reach goals
	Solver->RootBehavior = EPBIKRootBehavior::Free;
	// removing pull chain alpha on all goals "calms" the motion down, especially when retargeting arms
	Solver->PullChainAlpha = 0.0f;

	// assign bone settings from template
	const FAutoCharacterizer& Characterizer = IKRigController.GetAutoCharacterizer();
	const FTemplateHierarchy* TemplateHierarchy = Characterizer.GetKnownTemplateHierarchy(CharacterizeResults.BestTemplateName);
	if (!ensureAlways(TemplateHierarchy))
	{
		return;
	}
	const FAbstractHierarchy MeshAbstractHierarchy(Mesh);
	const TArray<FBoneSettingsForIK>& AllBoneSettings = TemplateHierarchy->AutoRetargetDefinition.BoneSettingsForIK.GetBoneSettings();
	for (const FBoneSettingsForIK& BoneSettings : AllBoneSettings)
	{
		// templates use "clean" names, free from prefixes, so we need to resolve this onto the actual skeletal mesh being setup
		const int32 BoneIndex = MeshAbstractHierarchy.GetBoneIndex(BoneSettings.BoneToApplyTo, ECleanOrFullName::Clean);
		if (BoneIndex == INDEX_NONE)
		{
			UE_LOG(LogIKRigEditor, Warning, TEXT("Auto FBIK using a template with settings for a bone that is not in this skeletal mesh: %s"), *BoneSettings.BoneToApplyTo.ToString());
			continue;
		}
		const FName BoneFullName = MeshAbstractHierarchy.GetBoneName(BoneIndex, ECleanOrFullName::Full);
		
		// optionally exclude bones
		if (BoneSettings.bExcluded)
		{
			IKRigController.SetBoneExcluded(BoneFullName, true /*bExclude*/);
		}

		// get the bone settings for this bone for the FBIK solver (or add one if there is none)
		auto GetOrAddBoneSettings = [SolverIndex, &IKRigController, BoneFullName]() -> UIKRig_FBIKBoneSettings*
		{
			if (!IKRigController.GetBoneSettings(BoneFullName, SolverIndex))
			{
				IKRigController.AddBoneSetting(BoneFullName, SolverIndex);
			}
			
			return CastChecked<UIKRig_FBIKBoneSettings>(IKRigController.GetBoneSettings(BoneFullName, SolverIndex));
		};

		// apply rotational stiffness
		if (BoneSettings.RotationStiffness > 0.f)
		{
			UIKRig_FBIKBoneSettings* Settings = GetOrAddBoneSettings();
			Settings->RotationStiffness = FMath::Clamp(BoneSettings.RotationStiffness, 0.f, 1.0f);
		}
		
		// apply preferred angles
		if (BoneSettings.PreferredAxis != EPreferredAxis::None)
		{
			UIKRig_FBIKBoneSettings* Settings = GetOrAddBoneSettings();
			Settings->bUsePreferredAngles = true;
			Settings->PreferredAngles = BoneSettings.GetPreferredAxisAsAngles();
		}
		
		// apply locked axes
		if (BoneSettings.bIsHinge)
		{
			UIKRig_FBIKBoneSettings* Settings = GetOrAddBoneSettings();
			BoneSettings.LockNonPreferredAxes(Settings->X, Settings->Y, Settings->Z);	
		}
	}
}

#undef LOCTEXT_NAMESPACE
