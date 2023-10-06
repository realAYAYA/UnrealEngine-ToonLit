// Copyright Epic Games, Inc. All Rights Reserved.

#include "CQTest.h"

TEST_CLASS(CommandBuilderTests, "TestFramework.CQTest.Core")
{
	FTestCommandBuilder CommandBuilder{*TestRunner};

	TEST_METHOD(Do_ThenBuild_IncludesCommand)
	{
		bool invoked = false;
		auto command = CommandBuilder.Do([&invoked]() { invoked = true; }).Build();

		ASSERT_THAT(IsTrue(command->Update()));
		ASSERT_THAT(IsTrue(invoked));
	}

	TEST_METHOD(Build_WithoutCommands_ReturnsNullptr)
	{
		auto command = CommandBuilder.Build();
		ASSERT_THAT(IsNull(command));
	}

	TEST_METHOD(StartWhen_CreatesWaitUntilCommand)
	{
		bool done = false;
		auto command = CommandBuilder.StartWhen([&done]() { return done; }).Build();

		ASSERT_THAT(IsFalse(command->Update()));
		done = true;
		ASSERT_THAT(IsTrue(command->Update()));
	}

	TEST_METHOD(Build_AfterBuild_ReturnsNullptr)
	{
		auto command = CommandBuilder.Do([]() {}).Build();
		auto secondTime = CommandBuilder.Build();

		ASSERT_THAT(IsNotNull(command));
		ASSERT_THAT(IsNull(secondTime));
	}
};