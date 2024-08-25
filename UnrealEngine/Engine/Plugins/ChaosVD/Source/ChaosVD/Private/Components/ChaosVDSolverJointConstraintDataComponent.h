// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "ChaosVDSolverJointConstraintDataComponent.generated.h"

struct FChaosVDJointConstraint;

typedef TMap<int32, TArray<TSharedPtr<FChaosVDJointConstraint>>> FChaosVDJointDataByParticleMap;
typedef TMap<int32, TSharedPtr<FChaosVDJointConstraint>> FChaosVDJointDataByConstraintIndexMap;
typedef TArray<TSharedPtr<FChaosVDJointConstraint>> FChaosVDJointDataArray;

enum class EChaosVDParticlePairSlot : uint8;

/** Struct used to pass data about a specific joint constraint to other objects */
struct FChaosVDJointConstraintSelectionHandle
{
	FChaosVDJointConstraintSelectionHandle()
	{		
	}

	FChaosVDJointConstraintSelectionHandle(const TWeakPtr<FChaosVDJointConstraint>& InJointData)
		: JointData(InJointData)
	{
	}

	void SetIsSelected(bool bNewSelected);
	bool IsSelected() const;

	bool IsValid();

	TWeakPtr<FChaosVDJointConstraint> GetData() const { return JointData; } 

private:
	TWeakPtr<FChaosVDJointConstraint> JointData = nullptr;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FChaosVDJointSlectionChangedDelegate, const FChaosVDJointConstraintSelectionHandle& SelectionHandle)

UCLASS()
class CHAOSVD_API UChaosVDSolverJointConstraintDataComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UChaosVDSolverJointConstraintDataComponent();

	void UpdateConstraintData(const TArray<TSharedPtr<FChaosVDJointConstraint>>& InJointData);
	
	const FChaosVDJointDataArray& GetAllJointConstraints() const { return AllJointConstraints; }
	const FChaosVDJointDataArray* GetJointConstraintsForParticle(int32 ParticleID, EChaosVDParticlePairSlot Options) const;

	void SelectJoint(int32 ConstraintIndex);
	void SelectJoint(const FChaosVDJointConstraintSelectionHandle& SelectionHandle);

	bool IsJointSelected(int32 ConstraintIndex) const;

	FChaosVDJointSlectionChangedDelegate& OnSelectionChanged() { return SelectionChangeDelegate; }

	const FChaosVDJointConstraintSelectionHandle& GetCurrentSelectionHandle() const { return CurrentJointSelectionHandle; }

protected:

	TSharedPtr<FChaosVDJointConstraint> GetJointConstraintByIndex(int32 ConstraintIndex);
	
	void ClearData();

	FChaosVDJointDataArray AllJointConstraints;
	
	FChaosVDJointDataByParticleMap JointConstraintByParticle0;
	FChaosVDJointDataByParticleMap JointConstraintByParticle1;

	FChaosVDJointDataByConstraintIndexMap JointConstraintByConstraintIndex;

	FChaosVDJointSlectionChangedDelegate SelectionChangeDelegate;

	FChaosVDJointConstraintSelectionHandle CurrentJointSelectionHandle;
};
