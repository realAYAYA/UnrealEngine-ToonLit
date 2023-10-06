// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Algo/Reverse.h"
#include "Misc/AutomationTest.h"
#include "Commands/TestCommands.h"

class FTestCommandBuilder
{
public:
	FTestCommandBuilder(FAutomationTestBase& InTestRunner)
		: TestRunner(InTestRunner) {}

	~FTestCommandBuilder()
	{
		checkf(CommandQueue.IsEmpty(), TEXT("Adding latent actions from within latent actions is currently unsupported."));
	}

	FTestCommandBuilder& Do(const TCHAR* Description, TFunction<void()> Action)
	{
		if (!TestRunner.HasAnyErrors())
		{
			CommandQueue.Add(MakeShared<FExecute>(TestRunner, Action, Description));
		}
		return *this;
	}

	FTestCommandBuilder& Do(TFunction<void()> Action)
	{
		return Do(nullptr, Action);
	}

	FTestCommandBuilder& Then(TFunction<void()> Action)
	{
		return Do(Action);
	}

	FTestCommandBuilder& Then(const TCHAR* Description, TFunction<void()> Action)
	{
		return Do(Description, Action);
	}

	FTestCommandBuilder& Until(const TCHAR* Description, TFunction<bool()> Query, FTimespan Timeout = FTimespan::FromSeconds(10))
	{
		if (!TestRunner.HasAnyErrors())
		{
			CommandQueue.Add(MakeShared<FWaitUntil>(TestRunner, Query, Timeout, Description));
		}
		return *this;
	}

	FTestCommandBuilder& Until(TFunction<bool()> Query, FTimespan Timeout = FTimespan::FromSeconds(10))
	{
		return Until(nullptr, Query, Timeout);
	}

	FTestCommandBuilder& StartWhen(TFunction<bool()> Query, FTimespan Timeout = FTimespan::FromSeconds(10))
	{
		return Until(Query, Timeout);
	}

	FTestCommandBuilder& StartWhen(const TCHAR* Description, TFunction<bool()> Query, FTimespan Timeout = FTimespan::FromSeconds(10))
	{
		return Until(Description, Query, Timeout);
	}

	FTestCommandBuilder& OnTearDown(const TCHAR* Description, TFunction<void()> Action)
	{
		if (!TestRunner.HasAnyErrors())
		{
			TearDownQueue.Add(MakeShared<FExecute>(TestRunner, Action, Description, ECQTestFailureBehavior::Run));
		}
		return *this;
	}

	FTestCommandBuilder& OnTearDown(TFunction<void()> Action)
	{
		return OnTearDown(nullptr, Action);
	}

	FTestCommandBuilder& CleanUpWith(const TCHAR* Description, TFunction<void()> Action)
	{
		return OnTearDown(Description, Action);
	}

	FTestCommandBuilder& CleanUpWith(TFunction<void()> Action)
	{
		return OnTearDown(nullptr, Action);
	}

	TSharedPtr<IAutomationLatentCommand> Build()
	{
		return BuildQueue(CommandQueue);
	}

	TSharedPtr<IAutomationLatentCommand> BuildTearDown()
	{
		// Last in, first out
		Algo::Reverse(TearDownQueue);
		return BuildQueue(TearDownQueue);
	}

private:
	TSharedPtr<IAutomationLatentCommand> BuildQueue(TArray<TSharedPtr<IAutomationLatentCommand>>& Queue)
	{
		TSharedPtr<IAutomationLatentCommand> Result = nullptr;
		if (Queue.Num() == 0)
		{
			return Result;
		}
		else if (Queue.Num() == 1)
		{
			Result = Queue[0];
		}
		else
		{
			Result = MakeShared<FRunSequence>(Queue);
		}

		Queue.Empty();
		return Result;
	}

protected:
	TArray<TSharedPtr<IAutomationLatentCommand>> CommandQueue{};
	TArray<TSharedPtr<IAutomationLatentCommand>> TearDownQueue{};
	FAutomationTestBase& TestRunner;

	template<typename Asserter>
	friend struct TBaseTest;
};