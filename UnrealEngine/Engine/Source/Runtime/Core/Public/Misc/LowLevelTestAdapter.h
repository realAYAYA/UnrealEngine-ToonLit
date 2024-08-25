// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if !WITH_LOW_LEVEL_TESTS

#include "Containers/StringConv.h"
#include "Containers/UnrealString.h"
#include <sstream>
#include <vector>

/**
* @brief Captures expressions and their evaluated values.
* Internal use only for the low level tests adapter.
*
* @param	InExpressions	Comma-separated list of expressions
* @param	InExperssionsValues	The list of evaluated expressions values
*/
template<typename... ArgTypes>
FString CaptureExpressionsAndValues(const FString& InExpressions, ArgTypes&&... InExpressionsValues)
{
	std::ostringstream Result;
	auto Args = { InExpressionsValues... };
	auto Iter = Args.begin();

	FString RemainingExpressions = InExpressions;
	FString Expression;
	while (RemainingExpressions.Split(TEXT(","), &Expression, &RemainingExpressions))
	{
		if (Iter == Args.end())
		{
			break;
		}
		else if (Iter != Args.begin())
		{
			Result << ", ";
		}
		Result << std::string(TCHAR_TO_UTF8(*Expression.TrimStartAndEnd())) << " = "  << *Iter++;
	}

	if (Iter != Args.end() && !RemainingExpressions.TrimStartAndEnd().IsEmpty())
	{
		Result << ", " << std::string(TCHAR_TO_UTF8(*RemainingExpressions)) << " = " << *Iter;
	}

	Result << std::endl;
	return FString(Result.str().c_str());
}


#define IMPLEMENT_SIMPLE_AUTOMATION_TEST_PRIVATE_LLT( TClass, PrettyName, TFlags, FileName, LineNumber ) \
		class TClass : public FAutomationTestBase \
		{ \
		public:\
			TClass( const FString& InName) \
			: FAutomationTestBase( InName, false ) \
			{ \
				TestFlags = ExtractAutomationTestFlags(TFlags); \
				PrettyNameDotNotation = FString(PrettyName).Replace(TEXT("::"), TEXT(".")); \
				if (!(TestFlags & EAutomationTestFlags::ApplicationContextMask)) \
				{ \
					TestFlags |= EAutomationTestFlags::ApplicationContextMask; \
				} \
				if (!(TestFlags & EAutomationTestFlags::FilterMask)) \
				{ \
					TestFlags |= EAutomationTestFlags::EngineFilter; \
				} \
			} \
			virtual uint32 GetTestFlags() const override { return TestFlags; } \
			virtual bool IsStressTest() const { return false; } \
			virtual uint32 GetRequiredDeviceNum() const override { return 1; } \
			virtual FString GetTestSourceFileName() const override { return FileName; } \
			virtual int32 GetTestSourceFileLine() const override { return LineNumber; } \
		protected: \
			virtual void GetTests(TArray<FString>& OutBeautifiedNames, TArray <FString>& OutTestCommands) const override \
			{ \
				OutBeautifiedNames.Add(PrettyNameDotNotation); \
				OutTestCommands.Add(FString());\
			} \
			void TestBody(const FString& Parameters); \
			virtual bool RunTest(const FString& Parameters) { \
				TestBody(Parameters); \
				return !HasAnyErrors(); \
			} \
			virtual FString GetBeautifiedTestName() const override { return PrettyNameDotNotation; } \
		private:\
			uint32 TestFlags; \
			FString PrettyNameDotNotation; \
		};

#define LLT_JOIN(Prefix, Counter) LLT_JOIN_INNER(Prefix, Counter)
#define LLT_JOIN_INNER(Prefix, Counter) Prefix##Counter

#define TEST_CASE_NAMED_STR(TClass, StrName, PrettyName, TFlags) \
		IMPLEMENT_SIMPLE_AUTOMATION_TEST_PRIVATE_LLT(TClass, PrettyName, TFlags, __FILE__, __LINE__) \
		namespace \
		{ \
			TClass LLT_JOIN(TClass, Instance)(TEXT(StrName)); \
		} \
		void TClass::TestBody(const FString& Parameters)


#define TEST_CASE_GENERATED_NAME_UNIQUE LLT_JOIN(FLLTAdaptedTest, __COUNTER__)
#define LLT_STR(Macro) #Macro
#define LLT_STR_EXPAND(Macro) LLT_STR(Macro)
#define TEST_CASE_GENERATED_NAME_UNIQUE_STR LLT_STR_EXPAND(TEST_CASE_GENERATED_NAME_UNIQUE)
// Note: TEST_CASE uses unique names which only work when used inside unique namespace in the same compilation unit?// Use TEST_CASE_NAMED instead and provide an unique global instance name
#define TEST_CASE(PrettyName, TFlags) TEST_CASE_NAMED_STR(TEST_CASE_GENERATED_NAME_UNIQUE, TEST_CASE_GENERATED_NAME_UNIQUE_STR, PrettyName, TFlags)
#define TEST_CASE_NAMED(ClassName, PrettyName, TFlags) TEST_CASE_NAMED_STR(ClassName, #ClassName, PrettyName, TFlags)

//-V:CHECK:571,501,547
#define CHECK(Expr) if (!(Expr)) { FAutomationTestFramework::Get().GetCurrentTest()->AddError(TEXT("Condition failed")); }
//-V:CHECK_FALSE:571,501,547
#define CHECK_FALSE(Expr) if (Expr) { FAutomationTestFramework::Get().GetCurrentTest()->AddError(TEXT("Condition expected to return false but returned true")); }
#define CHECKED_IF(Expr) if (Expr)
#define CHECKED_ELSE(Expr) if (!(Expr))
//-V:CHECK_MESSAGE:571,501,547
#define CHECK_MESSAGE(Message, Expr) if (!(Expr)) { FAutomationTestFramework::Get().GetCurrentTest()->AddError(Message); }
//-V:CHECK_FALSE_MESSAGE:571,501,547
#define CHECK_FALSE_MESSAGE(Message, Expr) if (Expr) { FAutomationTestFramework::Get().GetCurrentTest()->AddError(Message); }
//-V:REQUIRE:571,501,547
#define REQUIRE(Expr) if (!(Expr)) { FAutomationTestFramework::Get().GetCurrentTest()->AddError(TEXT("Required condition failed, interrupting test")); return; }
//-V:REQUIRE_MESSAGE:571,501,547
#define REQUIRE_MESSAGE(Message, Expr) if (!(Expr)) { FAutomationTestFramework::Get().GetCurrentTest()->AddError(Message); return; }
#define STATIC_REQUIRE(...) static_assert(__VA_ARGS__, #__VA_ARGS__);
#define STATIC_CHECK(...) static_assert(__VA_ARGS__, #__VA_ARGS__);
#define STATIC_CHECK_FALSE(...) static_assert(!(__VA_ARGS__), "!(" #__VA_ARGS__ ")");

#define CHECK_EQUALS(What, X, Y) FAutomationTestFramework::Get().GetCurrentTest()->TestEqual(What, X, Y);
#define CHECK_NOT_EQUALS(What, X, Y) FAutomationTestFramework::Get().GetCurrentTest()->TestNotEqual(What, X, Y);

#define SECTION(Text) FAutomationTestFramework::Get().GetCurrentTest()->AddInfo(TEXT(Text));
#define FAIL_CHECK(Message) FAutomationTestFramework::Get().GetCurrentTest()->AddError(Message);

#define CAPTURE(...) FAutomationTestFramework::Get().GetCurrentTest()->AddInfo(CaptureExpressionsAndValues(#__VA_ARGS__, __VA_ARGS__));
#define INFO(Message) FAutomationTestFramework::Get().GetCurrentTest()->AddInfo(Message);
#define WARN(Message) FAutomationTestFramework::Get().GetCurrentTest()->AddWarning(Message); 
#define FAIL_ON_MESSAGE(Message) FAutomationTestFramework::Get().GetCurrentTest()->AddExpectedError(Message);

#define SKIP(Message)

#endif // !WITH_LOW_LEVEL_TESTS