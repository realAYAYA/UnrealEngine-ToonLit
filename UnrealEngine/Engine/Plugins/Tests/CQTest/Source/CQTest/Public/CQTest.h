// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/Engine.h"

#include "Misc/AutomationTest.h"
#include "HAL/Platform.h"

#include "Commands/TestCommands.h"
#include "Commands/TestCommandBuilder.h"

#include "Assert/NoDiscardAsserter.h"

/*
//Example boiler plate

#include "CQTest.h"
//TEST_CLASS(MyFixtureName, GenerateTestDirectory)
TEST_CLASS(MyFixtureName, "Game.Example")
{
	//Member variables shared between tests

	BEFORE_EACH()
	{
		//delete if empty
	}

	AFTER_EACH()
	{
		//delete if empty
	}

	TEST_METHOD(When_Given_Expect)
	{
		ASSERT_THAT(IsTrue(false));
	}
};
*/

namespace TestDirectoryGenerator
{
	CQTEST_API FString Generate(const FString& Filename);
}

enum class ECQTestSuppressLogBehavior
{
	Default,
	True,
	False
};

static const FString GenerateTestDirectory = TEXT("");
static constexpr uint32 DefaultFlags = EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter;

template <typename AsserterType>
struct TBaseTest
{
	TBaseTest(FAutomationTestBase& TestRunner, bool bInitializing);
	TBaseTest(const TBaseTest<AsserterType>& other) = delete;
	TBaseTest& operator=(const TBaseTest<AsserterType>& rhs) = delete;

	virtual ~TBaseTest() = default;

	virtual void Setup() {}

	virtual void TearDown() {}

	// The framework expects you to pass a 'new' command, which it will own and destroy
	void AddCommand(IAutomationLatentCommand* Cmd);
	void AddCommand(TSharedPtr<IAutomationLatentCommand> Cmd);

	virtual void RunTest(const FString& MethodName) = 0;

	bool bInitializing{ true };

	FAutomationTestBase& TestRunner;
	AsserterType Assert;
	FTestCommandBuilder TestCommandBuilder;
};

template <typename AsserterType>
struct TTestRunner;

template <typename AsserterType>
using TTestInstanceGenerator = TUniquePtr<TBaseTest<AsserterType>> (*)(TTestRunner<AsserterType>&);

template <typename AsserterType>
struct TTestRunner : public FAutomationTestBase
{
	TTestRunner(FString Name, int32 LineNumber, const char* FileName, FString TestDir, uint32 TestFlags, TTestInstanceGenerator<AsserterType> Factory);

	uint32 GetTestFlags() const override;
	FString GetTestSourceFileName() const override;
	int32 GetTestSourceFileLine() const override;
	int32 GetTestSourceFileLine(const FString& Name) const override;
	bool SuppressLogWarnings() override;
	bool SuppressLogErrors() override;
	void GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const override;

	bool RunTest(const FString& RequestedTest) override;

	virtual void SetSuppressLogWarnings(ECQTestSuppressLogBehavior Behavior = ECQTestSuppressLogBehavior::True);
	virtual void SetSuppressLogErrors(ECQTestSuppressLogBehavior Behavior = ECQTestSuppressLogBehavior::True);

	int32 LineNumber = 0;
	FString FileName;
	FString TestDir;
	uint32 TestFlags{ DefaultFlags };
	bool bInitializing{ true };

	TUniquePtr<TBaseTest<AsserterType>> CurrentTestPtr;
	TArray<FString> TestNames;
	TMap<FString, int32> TestLineNumbers{};
	TTestInstanceGenerator<AsserterType> TestInstanceFactory;

protected:
	FString GetBeautifiedTestName() const override;
	uint32 GetRequiredDeviceNum() const override;

	ECQTestSuppressLogBehavior SuppressLogWarningsBehavior{ ECQTestSuppressLogBehavior::Default };
	ECQTestSuppressLogBehavior SuppressLogErrorsBehavior{ ECQTestSuppressLogBehavior::Default };

};

template <typename Derived, typename AsserterType>
struct TTest : TBaseTest<AsserterType>
{
	using TestMethod = void (Derived::*)();
	using DerivedType = Derived;

	TTest()
		: TBaseTest<AsserterType>(*Derived::TestRunner, Derived::TestRunner->bInitializing) {}

	void RunTest(const FString& TestName) override;

	struct FFunctionRegistrar
	{
		FFunctionRegistrar(FString Name, TestMethod Func, int32 LineNumber)
		{
			DerivedType::TestRunner->TestNames.Add(Name);
			DerivedType::Methods.Add(Name, Func);
			DerivedType::TestRunner->TestLineNumbers.Add(Name, LineNumber);
		}
	};

	static TUniquePtr<TBaseTest<AsserterType>> CreateTestClass(TTestRunner<AsserterType>& TestRunnerInstance)
	{
		DerivedType::TestRunner = &TestRunnerInstance;
		return MakeUnique<DerivedType>();
	}

	inline static TMap<FString, TestMethod> Methods{};
	inline static TTestRunner<AsserterType>* TestRunner{ nullptr };
};

#if WITH_AUTOMATION_WORKER
	#define _TEST_CLASS_IMPL(_ClassName, _TestDir, _BaseClass, _AsserterType, _TestFlags)                                                  \
		struct _ClassName;                                                                                                                 \
		struct F##_ClassName##_Runner : public TTestRunner<_AsserterType>                                                                  \
		{                                                                                                                                  \
			F##_ClassName##_Runner()                                                                                                       \
				: TTestRunner(#_ClassName, __LINE__, __FILE__, _TestDir, _TestFlags, TTest<_ClassName, _AsserterType>::CreateTestClass) {} \
		};                                                                                                                                 \
		F##_ClassName##_Runner _ClassName##_RunnerInstance;                                                                                \
		struct _ClassName : public _BaseClass<_ClassName, _AsserterType>

	#define TEST_METHOD(_MethodName)                                                                       \
		FFunctionRegistrar reg##_MethodName{ FString(#_MethodName), &DerivedType::_MethodName, __LINE__ }; \
	void _MethodName()
#else
	#define _TEST_CLASS_IMPL(_ClassName, _TestDir, _BaseClass, _AsserterType, _TestFlags) \
			struct _ClassName : public _BaseClass<_ClassName, _AsserterType>

	#define TEST_METHOD(_MethodName) void _MethodName()
#endif

#define TEST_CLASS_WITH_ASSERTS(_ClassName, _TestDir, _AsserterType) _TEST_CLASS_IMPL(_ClassName, _TestDir, TTest, _AsserterType, DefaultFlags)

#define TEST_CLASS(_ClassName, _TestDir) TEST_CLASS_WITH_ASSERTS(_ClassName, _TestDir, FNoDiscardAsserter)
#define TEST_CLASS_WITH_BASE(_ClassName, _TestDir, _BaseClass) _TEST_CLASS_IMPL(_ClassName, _TestDir, _BaseClass, FNoDiscardAsserter, DefaultFlags)
#define TEST_CLASS_WITH_FLAGS(_ClassName, _TestDir, _Flags) _TEST_CLASS_IMPL(_ClassName, _TestDir, TTest, FNoDiscardAsserter, _Flags)
#define TEST_CLASS_WITH_BASE_AND_FLAGS(_ClassName, _TestDir, _BaseClass, _Flags) _TEST_CLASS_IMPL(_ClassName, _TestDir, _BaseClass, FNoDiscardAsserter, _Flags)

#define TEST(_TestName, _TestDir)        \
	TEST_CLASS(_TestName, _TestDir)      \
	{                                    \
		TEST_METHOD(_TestName##_Method); \
	};                                   \
	void _TestName::_TestName##_Method()

#define ASSERT_THAT(_assertion) if (!Assert._assertion) {return;}
#define ASSERT_FAIL(_Msg) Assert.Fail(_Msg); return;

#define BEFORE_EACH() virtual void Setup() override
#define AFTER_EACH() virtual void TearDown() override

#include "Impl/CQTest.inl"
