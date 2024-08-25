// Copyright Epic Games, Inc. All Rights Reserved.

#include "Trace/DataProcessors/ChaosVDJointConstraintDataProcessor.h"

#include "DataWrappers/ChaosVDJointDataWrappers.h"
#include "Trace/ChaosVDTraceProvider.h"


FChaosVDJointConstraintDataProcessor::FChaosVDJointConstraintDataProcessor()
	: IChaosVDDataProcessor(FChaosVDJointConstraint::WrapperTypeName)
{
}

bool FChaosVDJointConstraintDataProcessor::ProcessRawData(const TArray<uint8>& InData)
{
	TSharedPtr<FChaosVDTraceProvider> ProviderSharedPtr = TraceProvider.Pin();
	if (!ensure(ProviderSharedPtr.IsValid()))
	{
		return false;
	}

	TSharedPtr<FChaosVDJointConstraint> JointConstraint = MakeShared<FChaosVDJointConstraint>();
	const bool bSuccess = Chaos::VisualDebugger::ReadDataFromBuffer(InData, *JointConstraint, ProviderSharedPtr.ToSharedRef());

	if (bSuccess)
	{
		// This can be null if the recording started Mid-Frame. In this case we just discard the data for now
		if (FChaosVDSolverFrameData* FrameData = ProviderSharedPtr->GetCurrentSolverFrame(JointConstraint->SolverID))
		{
			if (ensureMsgf(FrameData->SolverSteps.Num() > 0, TEXT("A Joint Constraint was traced without a valid step scope")))
			{
				FrameData->SolverSteps.Last().RecordedJointConstraints.Add(JointConstraint);
			}
		}
	}

	return bSuccess;
}
