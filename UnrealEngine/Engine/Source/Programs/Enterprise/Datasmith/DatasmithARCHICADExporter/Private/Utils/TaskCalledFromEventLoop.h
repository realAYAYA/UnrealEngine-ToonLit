// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AddonTools.h"

BEGIN_NAMESPACE_UE_AC

// Schedule an idle task
class FTaskCalledFromEventLoop
{
  public:
	// Destructor
	virtual ~FTaskCalledFromEventLoop() {}

	// Run the task
	virtual void Run() = 0;

	// If the task is retain or not
	enum ERetainType
	{
		kSharedRef = 0, // Retain the task
		kWeakPtr // The task is retain by another reference or we will not call it
	};

	// Schedule InTask to be executed on next event.
	static void CallTaskFromEventLoop(const TSharedRef< FTaskCalledFromEventLoop >& InTask, ERetainType InRetainType);

	// Schedule functor to be executed on next event.
	template < typename Functor > static void CallFunctorFromEventLoop(Functor InFunctor);

	// Register the task service
	static GSErrCode Register();

	// Initialize
	static GSErrCode Initialize();

	// Uninitialize the task service
	static void Uninitialize();

  private:
	static GSErrCode DoTasks(GSHandle ParamHandle);
	static GSErrCode DoTasksCallBack(GSHandle ParamHandle, GSPtr OutResultData, bool bSilentMode);
};

// Template class for function
template < typename Functor > class TFunctorCalledFromEventLoop : public FTaskCalledFromEventLoop
{
  public:
	// Constructor
	TFunctorCalledFromEventLoop(Functor InFunctor)
		: TheFunctor(InFunctor)
	{
	}

	// Execute the functor
	virtual void Run() override { TheFunctor(); }

  private:
	Functor TheFunctor;
};

// Implementation of schedule functor to be executed on next event.
template < typename Functor > void FTaskCalledFromEventLoop::CallFunctorFromEventLoop(Functor InFunctor)
{
	CallTaskFromEventLoop(MakeShared< TFunctorCalledFromEventLoop< Functor > >(InFunctor), kSharedRef);
}

END_NAMESPACE_UE_AC
