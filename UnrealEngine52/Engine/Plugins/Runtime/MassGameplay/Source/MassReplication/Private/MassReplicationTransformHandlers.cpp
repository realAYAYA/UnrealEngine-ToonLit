// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassReplicationTransformHandlers.h"
#include "MassEntityQuery.h"
#include "MassCommonFragments.h"
#include "MassExecutionContext.h"
#include "AIHelpers.h"

void FMassReplicationProcessorTransformHandlerBase::AddRequirements(FMassEntityQuery& InQuery)
{
	InQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
}

void FMassReplicationProcessorTransformHandlerBase::CacheFragmentViews(FMassExecutionContext& ExecContext)
{
	TransformList = ExecContext.GetMutableFragmentView<FTransformFragment>();
}

void FMassReplicationProcessorPositionYawHandler::AddEntity(const int32 EntityIdx, FReplicatedAgentPositionYawData& InOutReplicatedPositionYawData) const
{
	const FTransformFragment& TransformFragment = TransformList[EntityIdx];
	InOutReplicatedPositionYawData.SetPosition(TransformFragment.GetTransform().GetLocation());

	const FRotator::FReal Yaw = FMath::DegreesToRadians(TransformFragment.GetTransform().Rotator().Yaw);
	InOutReplicatedPositionYawData.SetYaw(static_cast<float>(Yaw));
}
