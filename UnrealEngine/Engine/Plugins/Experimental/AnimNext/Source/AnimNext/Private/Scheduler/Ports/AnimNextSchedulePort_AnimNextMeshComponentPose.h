// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Scheduler/AnimNextSchedulePort.h"
#include "AnimNextSchedulePort_AnimNextMeshComponentPose.generated.h"

UCLASS(DisplayName = "AnimNext Mesh Component Pose")
class UAnimNextSchedulePort_AnimNextMeshComponentPose : public UAnimNextSchedulePort
{
	GENERATED_BODY()

	// UAnimNextSchedulePort interface
	virtual void Run(const UE::AnimNext::FScheduleTermContext& InContext) const override;
	virtual TConstArrayView<FAnimNextParam> GetRequiredParameters() const override;
	
	// IAnimNextScheduleTermInterface interface
	virtual TConstArrayView<UE::AnimNext::FScheduleTerm> GetTerms() const override;

	static UE::AnimNext::FParamId ComponentParamId;
	static UE::AnimNext::FParamId ReferencePoseParamId;
};