// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if !WITH_LOW_LEVEL_TESTS

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"

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
				return true; \
			} \
			virtual FString GetBeautifiedTestName() const override { return PrettyNameDotNotation; } \
		private:\
			uint32 TestFlags; \
			FString PrettyNameDotNotation; \
		};

#define LLT_JOIN(Prefix, Counter) LLT_JOIN_INNER(Prefix, Counter)
#define LLT_JOIN_INNER(Prefix, Counter) Prefix##Counter

#define TEST_CASE_NAMED(TClass, StrName, PrettyName, TFlags) \
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
#define TEST_CASE(PrettyName, TFlags) TEST_CASE_NAMED(TEST_CASE_GENERATED_NAME_UNIQUE, TEST_CASE_GENERATED_NAME_UNIQUE_STR, PrettyName, TFlags)

#define CHECK(Expr) if (!(Expr)) { FAutomationTestFramework::Get().GetCurrentTest()->AddError(TEXT("Condition failed")); }
#define CHECK_FALSE(Expr) if (Expr) { FAutomationTestFramework::Get().GetCurrentTest()->AddError(TEXT("Condition expected to return false but returned true")); }
#define REQUIRE(Expr) if (!(Expr)) { FAutomationTestFramework::Get().GetCurrentTest()->AddError(TEXT("Required condition failed, interrupting test")); return; }

#define SECTION(Text) AddInfo(TEXT(Text));

#endif // !WITH_LOW_LEVEL_TESTS