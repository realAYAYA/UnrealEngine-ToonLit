// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "TestDriver.h"
#include "Online/OnlineServicesCommon.h"
#include <catch2/catch_test_macros.hpp>
/*
* Class to protect unfilled promises made due to a test exiting early
* on a failed assert during a callback. This will make sure the promise created
* by this class is always filled out to at least be false.
*/
class TestSafePromiseDeleter
{
public:
	TestSafePromiseDeleter()
	{
		bPromiseCalled = false;
	}
	~TestSafePromiseDeleter()
	{
		if (!bPromiseCalled)
		{
			Promise.SetValue(false);
		}
	}
	void SetValue(bool Result)
	{
		bPromiseCalled = true;
		Promise.SetValue(Result);
	}
	TFuture<bool> GetFuture()
	{
		return Promise.GetFuture();
	}
protected:
	TPromise<bool> Promise;
	bool bPromiseCalled;
};
using FAsyncStepResult = TSharedPtr<TestSafePromiseDeleter>;

// Helper class for doing async test steps instead of normal tick-driven test steps
class FAsyncTestStep : public FTestPipeline::FStep
{
public:
	// main entry point function- OnlineSubsystem will be V2. The future bool in Result is whether or not to continue the test (essentually just a require)
	virtual void Run(FAsyncStepResult Result, SubsystemType OnlineSubsystem) = 0;

	virtual EContinuance Tick(SubsystemType OnlineSubsystem) override
	{
		if (!bInitialized)
		{
			bInitialized = true;
			FAsyncStepResult Result = MakeShared<TestSafePromiseDeleter>();
			Result->GetFuture().Next([this](bool Result)
			{
				REQUIRE(Result);
				bComplete = true;
			});
			Run(Result, OnlineSubsystem);
		}

		if (bComplete == true)
		{
			return EContinuance::Done;
		}
		else
		{
			return EContinuance::ContinueStepping;
		}
	}

protected:
	bool bInitialized = false;
	TAtomic<bool> bComplete = false;
};