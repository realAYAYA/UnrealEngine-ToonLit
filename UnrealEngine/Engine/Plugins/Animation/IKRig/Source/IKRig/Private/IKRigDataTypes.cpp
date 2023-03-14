// Copyright Epic Games, Inc. All Rights Reserved.

#include "IKRigDataTypes.h"
#include "IKRigDefinition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(IKRigDataTypes)

void FIKRigGoalContainer::SetIKGoal(const FIKRigGoal& InGoal)
{
	FIKRigGoal* Goal = FindGoalWriteable(InGoal.Name);
	if (!Goal)
	{
		// container hasn't seen this goal before, create new one, copying the input goal
		Goals.Emplace(InGoal);
		return;
	}

	// copy settings to existing goal
	*Goal = InGoal;
}

void FIKRigGoalContainer::SetIKGoal(const UIKRigEffectorGoal* InEffectorGoal)
{
	FIKRigGoal* Goal = FindGoalWriteable(InEffectorGoal->GoalName);
	if (!Goal)
	{
		// container hasn't seen this goal before, create new one, copying the Effector goal
		Goals.Emplace(InEffectorGoal);
		return;
	}

	// goals in editor have "preview mode" which allows them to be specified relative to their
	// initial pose to simulate an additive runtime behavior
#if WITH_EDITOR
	if (InEffectorGoal->PreviewMode == EIKRigGoalPreviewMode::Additive)
	{
		// transform to be interpreted as an offset, relative to input pose
		Goal->Position = InEffectorGoal->CurrentTransform.GetTranslation() - InEffectorGoal->InitialTransform.GetTranslation();
		const FQuat RelativeRotation = InEffectorGoal->CurrentTransform.GetRotation() * InEffectorGoal->InitialTransform.GetRotation().Inverse();
		Goal->Rotation = RelativeRotation.Rotator();
		Goal->PositionSpace = EIKRigGoalSpace::Additive;
		Goal->RotationSpace = EIKRigGoalSpace::Additive;
	}else
#endif
	{
		// transform to be interpreted as absolute and in component space
		Goal->Position = InEffectorGoal->CurrentTransform.GetTranslation();
		Goal->Rotation = InEffectorGoal->CurrentTransform.Rotator();
		Goal->PositionSpace = EIKRigGoalSpace::Component;
		Goal->RotationSpace = EIKRigGoalSpace::Component;
	}
	
    Goal->PositionAlpha = InEffectorGoal->PositionAlpha;
    Goal->RotationAlpha = InEffectorGoal->RotationAlpha;
}

const FIKRigGoal* FIKRigGoalContainer::FindGoalByName(const FName& GoalName) const
{
	return Goals.FindByPredicate([GoalName](const FIKRigGoal& Other)	{return Other.Name == GoalName;});
}

FIKRigGoal* FIKRigGoalContainer::FindGoalWriteable(const FName& GoalName) const
{
	return const_cast<FIKRigGoal*>(FindGoalByName(GoalName));
}

