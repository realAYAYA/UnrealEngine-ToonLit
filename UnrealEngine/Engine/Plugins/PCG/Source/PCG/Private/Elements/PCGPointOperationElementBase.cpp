// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGPointOperationElementBase.h"

#include "Data/PCGPointData.h"
#include "Helpers/PCGAsync.h"

#define LOCTEXT_NAMESPACE "PCGPointOperationElementBase"

bool FPCGPointOperationElementBase::PrepareDataInternal(FPCGContext* Context) const
{
	check(Context);
	ContextType* PointProcessContext = static_cast<ContextType*>(Context);
	check(PointProcessContext);

	// Prepares the context for time slicing
	return PreparePointOperationData(PointProcessContext);
}

bool FPCGPointOperationElementBase::PreparePointOperationData(ContextType* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGPointOperationElementBase::PreparePointProcessing);
	check(Context);

	const TArray<FPCGTaggedData>& Inputs = Context->InputData.GetInputs();
	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

	// There is no execution state, so this just flags that its okay to continue
	Context->InitializePerExecutionState();

	// Prepare the 'per iteration' time slice context state and allocate output point data
	Context->InitializePerIterationStates(Inputs.Num(),
		[this, &Context, &Inputs, &Outputs](IterStateType& OutState, const ExecStateType& ExecState, const uint32 IterationIndex)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FPCGPointOperationElementBase::InitializePerIterationStates);

			FPCGTaggedData& Output = Outputs.Add_GetRef(Inputs[IterationIndex]);

			const UPCGSpatialData* SpatialData = Cast<UPCGSpatialData>(Inputs[IterationIndex].Data);

			if (!SpatialData)
			{
				PCGE_LOG(Error, GraphAndLog, LOCTEXT("InputMissingSpatialData", "Unable to get Spatial data from input"));
				return EPCGTimeSliceInitResult::NoOperation;
			}

			OutState.InputPointData = SpatialData->ToPointData(Context);

			if (!OutState.InputPointData)
			{
				PCGE_LOG(Error, GraphAndLog, LOCTEXT("InputMissingPointData", "Unable to get Point data from input"));
				return EPCGTimeSliceInitResult::NoOperation;
			}

			OutState.NumPoints = OutState.InputPointData->GetPoints().Num();

			// Create and initialize the output points
			OutState.OutputPointData = NewObject<UPCGPointData>();
			OutState.OutputPointData->InitializeFromData(OutState.InputPointData);
			OutState.OutputPointData->GetMutablePoints().SetNumUninitialized(OutState.NumPoints);

			Output.Data = OutState.OutputPointData;

			return EPCGTimeSliceInitResult::Success;
		});

	return true;
}

bool FPCGPointOperationElementBase::ExecutePointOperation(ContextType* Context, TFunctionRef<PointExecSignature> PointFunction, int32 PointsPerChunk) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGPointOperationElementBase::ExecutePointProcessing);
	check(Context);

	// Standard check that the time slice state has been prepared. If the result is NoOp or Failure, result in no output
	if (!Context->DataIsPreparedForExecution() || Context->GetExecutionStateResult() != EPCGTimeSliceInitResult::Success)
	{
		Context->OutputData.TaggedData.Empty();
		return true;
	}

	return ExecuteSlice(Context, [this, &PointFunction, &PointsPerChunk](ContextType* Context, const ExecStateType& ExecState, const IterStateType& IterState, const uint32 IterIndex)
	{
		const TArray<FPCGPoint>& InPoints = IterState.InputPointData->GetPoints();
		TArray<FPCGPoint>& OutPoints = IterState.OutputPointData->GetMutablePoints();

		// If this input created an error, result in no output
		if (Context->GetIterationStateResult(IterIndex) != EPCGTimeSliceInitResult::Success)
		{
			OutPoints.Empty();
			return true;
		}

		// Conversion lambda from index to point ref for ease of use
		auto InternalPointFunction = [&PointFunction, &InPoints, &OutPoints](int32 ReadIndex, int32 WriteIndex)->bool
		{
			return PointFunction(InPoints[ReadIndex], OutPoints[WriteIndex]);
		};

		const bool bAsyncDone = FPCGAsync::AsyncProcessingOneToOneEx(
			&Context->AsyncState,
			IterState.NumPoints,
			/*InitializeFunc=*/[] {}, // Not useful for this context, since its preferred to initialize in PrepareDataInternal, so empty lambda
			std::move(InternalPointFunction),
			Context->TimeSliceIsEnabled(),
			PointsPerChunk);

		if (bAsyncDone)
		{
			PCGE_LOG_C(Verbose, LogOnly, Context, FText::Format(LOCTEXT("PointProcessInfo", "Processed {0} points"), IterState.NumPoints));
		}

		return bAsyncDone;
	});
}

#undef LOCTEXT_NAMESPACE
