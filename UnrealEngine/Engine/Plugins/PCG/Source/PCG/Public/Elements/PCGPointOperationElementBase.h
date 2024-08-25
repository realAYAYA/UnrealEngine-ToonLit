// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGTimeSlicedElementBase.h"

class UPCGPointData;
struct FPCGPoint;

namespace PCGPointOperation
{
	namespace Constants
	{
		/** The default minimum number of points to execute per async slice */
		static constexpr int32 PointsPerChunk = 4096;
	}

	/** Stores the input and output data as the state of the time sliced execution */
	struct IterationState
	{
		const UPCGPointData* InputPointData = nullptr;
		UPCGPointData* OutputPointData = nullptr;
		int32 NumPoints = 0;
	};
}

/** Simplified, time-sliced, and point by point operation class. A function or lambda may be passed into the `ExecutePointOperation` at execution time to
 * invoke a customized update operation on all incoming points, individually.
 */
class PCG_API FPCGPointOperationElementBase : public TPCGTimeSlicedElementBase<PCGTimeSlice::FEmptyStruct, PCGPointOperation::IterationState>
{
	using PointExecSignature = bool(const FPCGPoint& InPoint, FPCGPoint& OutPoint);

protected:
	//~Begin IPCGElement interface
	/** Conveniently calls PreparePointOperationData to prepare the time sliced element for execution. May be overridden, but PreparePointOperationData must be called. */
	virtual bool PrepareDataInternal(FPCGContext* Context) const override;
	//~End IPCGElement interface

	/** Executes the PointFunction function/lambda for every point copied from PreparePointOperationData */
	bool ExecutePointOperation(ContextType* Context, TFunctionRef<PointExecSignature> PointFunction, int32 PointsPerChunk = PCGPointOperation::Constants::PointsPerChunk) const;

private:
	/** Mandatory call. Using the context, prepares the state data for time slice execution */
	bool PreparePointOperationData(ContextType* Context) const;
};