// Copyright Epic Games, Inc. All Rights Reserved.

#include "LearningRecorderObject.h"

namespace UE::Learning
{
	FRecorderObject::FRecorderObject(
		const int32 InMaxInstanceNum,
		const int32 InMaxStepNum,
		const TSharedRef<FArrayMap>& InSrcArrayMap,
		const TSharedRef<FArrayMap>& InLogArrayMap)
		: MaxInstanceNum(InMaxInstanceNum)
		, MaxStepNum(InMaxStepNum)
		, SrcArrayMap(InSrcArrayMap)
		, LogArrayMap(InLogArrayMap)
	{
		StepNums.SetNumUninitialized({ MaxInstanceNum });
		Array::Zero(StepNums);
	}

	FRecorderObject::~FRecorderObject() { }

	void FRecorderObject::Reset(const FIndexSet Instances)
	{
		Array::Zero(StepNums, Instances);
	}

	void FRecorderObject::Update(const FIndexSet Instances)
	{
		const int32 VariableNum = SrcHandles.Num();

		for (int32 VariableIdx = 0; VariableIdx < VariableNum; VariableIdx++)
		{
			CopyFunctions[VariableIdx](
				StepNums,
				*SrcArrayMap,
				*LogArrayMap,
				SrcHandles[VariableIdx],
				LogHandles[VariableIdx],
				MaxStepNum,
				Instances);
		}
	}

}

