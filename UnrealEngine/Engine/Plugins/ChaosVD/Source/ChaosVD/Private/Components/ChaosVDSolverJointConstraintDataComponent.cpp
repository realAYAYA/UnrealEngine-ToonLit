// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/ChaosVDSolverJointConstraintDataComponent.h"

#include "Actors/ChaosVDSolverInfoActor.h"
#include "Components/ChaosVDConstraintDataHelpers.h"
#include "DataWrappers/ChaosVDJointDataWrappers.h"

void FChaosVDJointConstraintSelectionHandle::SetIsSelected(bool bNewSelected)
{
	if (const TSharedPtr<FChaosVDJointConstraint> JointDataPtr = JointData.Pin())
	{
		JointDataPtr->bIsSelectedInEditor = bNewSelected;
	}
}

bool FChaosVDJointConstraintSelectionHandle::IsSelected() const
{
	if (const TSharedPtr<FChaosVDJointConstraint> JointDataPtr = JointData.Pin())
	{
		return JointDataPtr->bIsSelectedInEditor;
	}

	return false;
}

UChaosVDSolverJointConstraintDataComponent::UChaosVDSolverJointConstraintDataComponent()
{
	SetCanEverAffectNavigation(false);
	bNavigationRelevant = false;
	PrimaryComponentTick.bCanEverTick = false;
}

void UChaosVDSolverJointConstraintDataComponent::UpdateConstraintData(const TArray<TSharedPtr<FChaosVDJointConstraint>>& InJointData)
{
	ClearData();

	AllJointConstraints = InJointData;

	JointConstraintByParticle0.Reserve(InJointData.Num());
	JointConstraintByParticle1.Reserve(InJointData.Num());
	JointConstraintByConstraintIndex.Reserve(InJointData.Num());

	for (const TSharedPtr<FChaosVDJointConstraint>& JointConstraint : InJointData)
	{
		if (!JointConstraint)
		{
			continue;
		}

		JointConstraintByConstraintIndex.Add(JointConstraint->ConstraintIndex, JointConstraint);
		Chaos::VisualDebugger::Utils::AddDataDataToParticleIDMap(JointConstraintByParticle0, JointConstraint, JointConstraint->ParticleParIndexes[0]);
		Chaos::VisualDebugger::Utils::AddDataDataToParticleIDMap(JointConstraintByParticle1, JointConstraint, JointConstraint->ParticleParIndexes[1]);
	}
}

const FChaosVDJointDataArray* UChaosVDSolverJointConstraintDataComponent::GetJointConstraintsForParticle(int32 ParticleID, EChaosVDParticlePairSlot Options) const
{
	return Chaos::VisualDebugger::Utils::GetDataFromParticlePairMaps<FChaosVDJointDataByParticleMap, TSharedPtr<FChaosVDJointConstraint>>(JointConstraintByParticle0, JointConstraintByParticle1, ParticleID, Options);
}

void UChaosVDSolverJointConstraintDataComponent::SelectJoint(int32 ConstraintIndex)
{
	CurrentJointSelectionHandle.SetIsSelected(false);
	CurrentJointSelectionHandle = FChaosVDJointConstraintSelectionHandle(GetJointConstraintByIndex(ConstraintIndex));
	CurrentJointSelectionHandle.SetIsSelected(true);

	SelectionChangeDelegate.Broadcast(CurrentJointSelectionHandle);
}

void UChaosVDSolverJointConstraintDataComponent::SelectJoint(const FChaosVDJointConstraintSelectionHandle& SelectionHandle)
{
	CurrentJointSelectionHandle.SetIsSelected(false);
	CurrentJointSelectionHandle = SelectionHandle;
	CurrentJointSelectionHandle.SetIsSelected(true);
	
	SelectionChangeDelegate.Broadcast(CurrentJointSelectionHandle);
}

bool UChaosVDSolverJointConstraintDataComponent::IsJointSelected(int32 ConstraintIndex) const
{
	if (const TSharedPtr<FChaosVDJointConstraint> JointDataPtr = CurrentJointSelectionHandle.GetData().Pin())
	{
		return JointDataPtr->bIsSelectedInEditor && JointDataPtr->ConstraintIndex == ConstraintIndex;
	}

	return false;
}

TSharedPtr<FChaosVDJointConstraint> UChaosVDSolverJointConstraintDataComponent::GetJointConstraintByIndex(int32 ConstraintIndex)
{
	if (TSharedPtr<FChaosVDJointConstraint>* JointConstraintPtrPtr = JointConstraintByConstraintIndex.Find(ConstraintIndex))
	{
		return *JointConstraintPtrPtr;
	}

	return nullptr;
}

void UChaosVDSolverJointConstraintDataComponent::ClearData()
{
	AllJointConstraints.Empty();
	JointConstraintByParticle0.Empty();
	JointConstraintByParticle1.Empty();
	JointConstraintByConstraintIndex.Empty();
}
