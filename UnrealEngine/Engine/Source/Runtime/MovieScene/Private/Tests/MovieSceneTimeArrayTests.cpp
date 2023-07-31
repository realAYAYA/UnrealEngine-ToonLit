// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Evaluation/MovieSceneTimeArray.h"
#include "Misc/AutomationTest.h"

#define LOCTEXT_NAMESPACE "MovieSceneTimeArrayTests"

template<typename DataType>
bool TestArrayEntries(FAutomationTestBase& Test, const TMovieSceneTimeArray<DataType>& TimeArray, const TArray<int32>& FrameNumbers, const TArray<DataType>& Data)
{
	if (!Test.TestEqual("Unexpected test arguments", Data.Num(), FrameNumbers.Num()))
	{
		return false;
	}

	TArrayView<const TMovieSceneTimeArrayEntry<DataType>> Entries = TimeArray.GetEntries();
	if (!Test.TestEqual("Unexpected number of entries", Entries.Num(), FrameNumbers.Num()))
	{
		return false;
	}

	for (int32 Index = 0, Num = Entries.Num(); Index < Num; ++Index)
	{
		const TMovieSceneTimeArrayEntry<DataType>& Entry(Entries[Index]);
		if (!Test.TestEqual(FString::Format(TEXT("Invalid time for entry {0}"), { Index }), Entry.RootTime, FFrameTime(FrameNumbers[Index])))
		{
			return false;
		}
		if (!Test.TestEqual(FString::Format(TEXT("Invalid data for entry {0}"), { Index }), Entry.Datum, Data[Index]))
		{
			return false;
		}
	}

	return true;
}

#define UTEST_TIME_ARRAY(TimeArray, FrameNumbers, Data)\
	if (!TestArrayEntries(*this, TimeArray, FrameNumbers, Data)) return false;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMovieSceneSimpleTimeArrayTests,
		"System.Engine.Sequencer.Core.SimpleTimeArrays",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMovieSceneSimpleTimeArrayTests::RunTest(const FString& Parameters)
{
	{
		TMovieSceneTimeArray<int32> Array;
		Array.Add(0, 0);
		Array.Add(10, 2);
		Array.Add(5, 1);
		UTEST_TIME_ARRAY(Array, TArray<int32>({ 0, 5, 10 }), TArray<int32>({ 0, 1, 2 }));
	}

	{
		TMovieSceneTimeArray<int32> Array;
		Array.Add(0, 0);
		Array.PushTransform(-200);
		Array.Add(0, 1);
		Array.Add(100, 2);
		Array.PopTransform();
		UTEST_TIME_ARRAY(Array, TArray<int32>({ 0, 200, 300 }), TArray<int32>({ 0, 1, 2 }));
	}

	return true;
}

#undef UTEST_TIME_ARRAY

#undef LOCTEXT_NAMESPACE
