// Copyright Epic Games, Inc. All Rights Reserved.

#include "CQTest.h"
TEST_CLASS(RunSequenceBasicTests, "TestFramework.CQTest.Core")
{
	TSharedPtr<IAutomationLatentCommand> Cmd1;
	TSharedPtr<IAutomationLatentCommand> Cmd2;
	FString Cmd1Name = "One";
	FString Cmd2Name = "Two";

	TArray<FString> Log;

	BEFORE_EACH() {
		Cmd1 = MakeShared<FExecute>(*TestRunner, [&]() { Log.Add(Cmd1Name); }, *Cmd1Name);
		Cmd2 = MakeShared<FExecute>(*TestRunner, [&]() { Log.Add(Cmd2Name); }, *Cmd2Name);
	}

	TEST_METHOD(Update_WithRemainingCommands_ReturnsFalse)
	{
		FRunSequence sequence(Cmd1, Cmd2);
		ASSERT_THAT(IsFalse(sequence.Update()));
	}

	TEST_METHOD(Update_OnLastCommand_ReturnTrue)
	{
		FRunSequence sequence(Cmd1);
		ASSERT_THAT(IsTrue(sequence.Update()));
	}

	TEST_METHOD(Append_ANewCommand_AddsCommandToEnd)
	{
		FRunSequence sequence{ Cmd1 };
		sequence.Append(Cmd2);

		sequence.Update();
		sequence.Update();

		ASSERT_THAT(AreEqual(2, Log.Num()));
		ASSERT_THAT(AreEqual(Cmd1Name, Log[0]));
		ASSERT_THAT(AreEqual(Cmd2Name, Log[1]));
	}

	TEST_METHOD(Prepend_ANewCommand_AddsCommandToBeginning)
	{
		FRunSequence sequence{ Cmd1 };
		sequence.Prepend(Cmd2);

		sequence.Update();
		sequence.Update();

		ASSERT_THAT(AreEqual(2, Log.Num()));
		ASSERT_THAT(AreEqual(Cmd2Name, Log[0]));
		ASSERT_THAT(AreEqual(Cmd1Name, Log[1]));
	}
};

struct FCommandLog
{
	TArray<FString> Commands;
};

class FNamedCommand : public IAutomationLatentCommand
{
public:
	FNamedCommand(TArray<FString>& CommandLog, FString Name)
		: Log(CommandLog), CommandName(Name) {}

	bool Update() override
	{
		Log.Add(CommandName);
		return true;
	}

	TArray<FString>& Log;
	FString CommandName;
};

class FTickingNamedCommand : public FNamedCommand
{
public:
	FTickingNamedCommand(TArray<FString>& CommandLog, FString Name, int32 Ticks)
		: FNamedCommand(CommandLog, Name), ExpectedCount(Ticks) {}

	bool Update() override
	{
		if (CurrentCount == ExpectedCount)
		{
			return true;
		}

		Log.Add(CommandName);
		CurrentCount++;
		return false;
	}

	int32 ExpectedCount{ 0 };
	int32 CurrentCount{ 0 };
};


TEST_CLASS(RunSequenceTests, "TestFramework.CQTest.Core")
{
	const TArray<FString> Names =
	{
		"Zero",
		"One",
		"Two",
		"Three",
		"Four",
		"Five",
		"Six",
		"Seven",
		"Eight"
	};

	TFunction<bool(RunSequenceTests*)> Assertion;
	TArray<FString> CommandLog;

	AFTER_EACH()
	{
		ASSERT_THAT(IsTrue(Assertion(this)));
	}

	TEST_METHOD(RunSequence_WithZeroCommands_DoesNotFail)
	{
		AddCommand(new FRunSequence());
		Assertion = [](RunSequenceTests* test) {
			return test->CommandLog.IsEmpty();
		};
	}

	TEST_METHOD(RunSequence_WithOneCommand_RunsCommand)
	{
		AddCommand(new FRunSequence(MakeShared<FNamedCommand>(CommandLog, Names[0])));
		Assertion = [](RunSequenceTests* test) {
			return test->CommandLog.Num() == 1 && test->CommandLog[0] == test->Names[0];
		};
	}

	TEST_METHOD(RunSequence_WithNamedCommands_RunsCommandsInOrder)
	{
		TArray<TSharedPtr<FNamedCommand>> Commands;
		Commands.Add(MakeShared<FNamedCommand>(CommandLog, Names[0]));
		Commands.Add(MakeShared<FNamedCommand>(CommandLog, Names[1]));
		Commands.Add(MakeShared<FNamedCommand>(CommandLog, Names[2]));
		Commands.Add(MakeShared<FNamedCommand>(CommandLog, Names[3]));
		Commands.Add(MakeShared<FNamedCommand>(CommandLog, Names[4]));

		AddCommand(new FRunSequence(Commands));

		Assertion = [](RunSequenceTests* test) {
			if (test->CommandLog.Num() != 5)
			{
				return false;
			}

			for (int32 i = 0; i < 5; i++)
			{
				if (test->CommandLog[i] != test->Names[i])
				{
					return false;
				}
			}

			return true;
		};
	}

	TEST_METHOD(RunSequence_WithTickingCommands_RunsCommandsInOrder)
	{
		TArray<TSharedPtr<FTickingNamedCommand>> Commands;
		Commands.Add(MakeShared<FTickingNamedCommand>(CommandLog, Names[0], 3));
		Commands.Add(MakeShared<FTickingNamedCommand>(CommandLog, Names[1], 3));
		Commands.Add(MakeShared<FTickingNamedCommand>(CommandLog, Names[2], 3));
		Commands.Add(MakeShared<FTickingNamedCommand>(CommandLog, Names[3], 3));
		Commands.Add(MakeShared<FTickingNamedCommand>(CommandLog, Names[4], 3));

		AddCommand(new FRunSequence(Commands));

		Assertion = [](RunSequenceTests* test) {
			if (test->CommandLog.Num() != 15)
			{
				return false;
			}

			int32 NameIndex = -1;
			for (int32 CommandIndex = 0; CommandIndex < test->CommandLog.Num(); CommandIndex++)
			{
				if (CommandIndex % 3 == 0)
				{
					NameIndex++;
				}

				if (test->CommandLog[CommandIndex] != test->Names[NameIndex])
				{
					return false;
				}
			}

			return true;
		};
	}

	TEST_METHOD(RunSequence_WithSequences_RunsCommandsInOrder)
	{
		TArray<TSharedPtr<FNamedCommand>> Cmds1;
		TArray<TSharedPtr<FNamedCommand>> Cmds2;
		TArray<TSharedPtr<FNamedCommand>> Cmds3;

		Cmds1.Add(MakeShared<FNamedCommand>(CommandLog, Names[0]));
		Cmds1.Add(MakeShared<FNamedCommand>(CommandLog, Names[1]));
		Cmds1.Add(MakeShared<FNamedCommand>(CommandLog, Names[2]));

		Cmds2.Add(MakeShared<FNamedCommand>(CommandLog, Names[3]));
		Cmds2.Add(MakeShared<FNamedCommand>(CommandLog, Names[4]));
		Cmds2.Add(MakeShared<FNamedCommand>(CommandLog, Names[5]));

		Cmds3.Add(MakeShared<FNamedCommand>(CommandLog, Names[6]));
		Cmds3.Add(MakeShared<FNamedCommand>(CommandLog, Names[7]));
		Cmds3.Add(MakeShared<FNamedCommand>(CommandLog, Names[8]));

		AddCommand(new FRunSequence(MakeShared<FRunSequence>(Cmds1), MakeShared<FRunSequence>(Cmds2), MakeShared<FRunSequence>(Cmds3)));

		Assertion = [](RunSequenceTests* test) {
			for (int32 i = 0; i < 9; i++)
			{
				if (test->CommandLog[i] != test->Names[i])
				{
					return false;
				}
			}

			return true;
		};
	}

	TEST_METHOD(RunSequence_WithSeparateSequences_RunsCommandsInOrder)
	{
		TArray<TSharedPtr<FNamedCommand>> Cmds1;
		TArray<TSharedPtr<FNamedCommand>> Cmds2;
		TArray<TSharedPtr<FNamedCommand>> Cmds3;

		Cmds1.Add(MakeShared<FNamedCommand>(CommandLog, Names[0]));
		Cmds1.Add(MakeShared<FNamedCommand>(CommandLog, Names[1]));
		Cmds1.Add(MakeShared<FNamedCommand>(CommandLog, Names[2]));

		Cmds2.Add(MakeShared<FNamedCommand>(CommandLog, Names[3]));
		Cmds2.Add(MakeShared<FNamedCommand>(CommandLog, Names[4]));
		Cmds2.Add(MakeShared<FNamedCommand>(CommandLog, Names[5]));

		Cmds3.Add(MakeShared<FNamedCommand>(CommandLog, Names[6]));
		Cmds3.Add(MakeShared<FNamedCommand>(CommandLog, Names[7]));
		Cmds3.Add(MakeShared<FNamedCommand>(CommandLog, Names[8]));

		AddCommand(new FRunSequence(Cmds1));
		AddCommand(new FRunSequence(Cmds2));
		AddCommand(new FRunSequence(Cmds3));

		Assertion = [](RunSequenceTests* test) {
			for (int32 i = 0; i < 9; i++)
			{
				if (test->CommandLog[i] != test->Names[i])
				{
					return false;
				}
			}

			return true;
		};
	}

	TEST_METHOD(RunSequence_WithUntilCommands_RunsCommandsInOrder)
	{
		TArray<TSharedPtr<IAutomationLatentCommand>> Cmds;
		Cmds.Add(MakeShared<FWaitUntil>(*TestRunner, [&]() {
			static int32 attempt = 0;
			CommandLog.Add(Names[0]);
			if (++attempt > 3)
			{
				attempt = 0;
				return true;
			}
			return false;
			}));
		Cmds.Add(MakeShared<FWaitUntil>(*TestRunner, [&]() {
			static int32 attempt = 0;
			CommandLog.Add(Names[1]);
			if (++attempt > 4)
			{
				attempt = 0;
				return true;
			}
			return false;
			}));

		AddCommand(new FRunSequence(Cmds));

		Assertion = [](RunSequenceTests* test) {
			return test->CommandLog.Num() == 9;
		};
	}
};