// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Templates/Function.h"
#include "Async/Async.h"


// sends InFunction to be called on the GameThread and waits for the result
template<typename RetType>
RetType CallOnGameThread(TFunction<RetType()> InFunction)
{
	TOptional<RetType> Result;
	TAtomic<bool> bCompleted(false);
	AsyncTask(ENamedThreads::GameThread, [&Result, &bCompleted, &InFunction]
	{
		Result = InFunction();
		bCompleted = true;
	});
	while (!bCompleted);
	return MoveTemp(Result.GetValue());
}

// fire (InFunction to be called on the GameThread) and forget
template<>
FORCEINLINE void CallOnGameThread(TFunction<void()> InFunction)
{
	AsyncTask(ENamedThreads::GameThread, InFunction);
}

