// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/AutomationTest.h"

class CQTEST_API FWaitUntil : public IAutomationLatentCommand
{
public:
	FWaitUntil(FAutomationTestBase& InTestRunner, TFunction<bool()> Query, FTimespan Timeout = FTimespan::FromSeconds(10), const TCHAR* InDescription = nullptr)
		: TestRunner(InTestRunner)
		, Query(MoveTemp(Query)) 
		, Timeout(Timeout)
		, Description(InDescription)
	{}

	bool Update() override;

	FAutomationTestBase& TestRunner;
	TFunction<bool()> Query;
	FTimespan Timeout;
	FDateTime StartTime;
	const TCHAR* Description;
	bool bHasTimerStarted = false;
};

enum class ECQTestFailureBehavior
{
	Skip,
	Run
};

class CQTEST_API FExecute : public IAutomationLatentCommand
{
public:
	FExecute(FAutomationTestBase& InTestRunner, TFunction<void()> Func, const TCHAR* InDescription = nullptr, ECQTestFailureBehavior InFailureBehavior = ECQTestFailureBehavior::Skip)
		: TestRunner(InTestRunner)
		, Func(MoveTemp(Func)) 
		, Description(InDescription)
		, FailureBehavior(InFailureBehavior)
	{}

	bool Update() override;

	FAutomationTestBase& TestRunner;
	TFunction<void()> Func;
	const TCHAR* Description = nullptr;
	ECQTestFailureBehavior FailureBehavior;
};

class CQTEST_API FRunSequence : public IAutomationLatentCommand
{
public:
	FRunSequence(const TArray<TSharedPtr<IAutomationLatentCommand>>& ToAdd)
		: Commands(ToAdd)
	{
	}

	template <class... Cmds>
	FRunSequence(Cmds... Commands)
		: FRunSequence(TArray<TSharedPtr<IAutomationLatentCommand>>{ Commands... })
	{
	}

	void Append(TSharedPtr<IAutomationLatentCommand> ToAdd);
	void AppendAll(TArray < TSharedPtr<IAutomationLatentCommand>> ToAdd);
	void Prepend(TSharedPtr<IAutomationLatentCommand> ToAdd);

	bool IsEmpty() const
	{
		return Commands.IsEmpty();
	}

	bool Update() override;

	TArray<TSharedPtr<IAutomationLatentCommand>> Commands;
};