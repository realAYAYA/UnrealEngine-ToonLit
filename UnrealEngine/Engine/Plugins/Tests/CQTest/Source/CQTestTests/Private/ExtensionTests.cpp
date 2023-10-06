// Copyright Epic Games, Inc. All Rights Reserved.

#include "CQTest.h"
#include "Assert/NoDiscardAsserter.h"

struct FCustomAsserter : public FNoDiscardAsserter
{
public:
	FCustomAsserter(FAutomationTestBase& testRunner)
		: FNoDiscardAsserter(testRunner)
	{
	}

	bool Custom(bool In) 
	{
		return In;
	}
};

#define CUSTOM_ASSERT_TEST_CLASS(_ClassName, _TestDir) TEST_CLASS_WITH_ASSERTS(_ClassName, _TestDir, FCustomAsserter)

CUSTOM_ASSERT_TEST_CLASS(CustomAsserts, "TestFramework.CQTest.Core")
{
	TEST_METHOD(CustomTestClass_WithCustomAsserter_HasInstanceOfCustomAsserter)
	{
		ASSERT_THAT(Custom(true));
		ASSERT_THAT(IsTrue(true));
	}
};

template<typename Derived, typename AsserterType>
struct TCustomBaseClass : public TTest<Derived, AsserterType>
{
	uint32 SpecialValue = 42;
};

#define CUSTOM_BASE_TEST_CLASS(_ClassName, _TestDir) TEST_CLASS_WITH_BASE(_ClassName, _TestDir, TCustomBaseClass)

CUSTOM_BASE_TEST_CLASS(CustomBase, "TestFramework.CQTest.Core")
{
	TEST_METHOD(CustomTestClass_WithCustomBase_InheritsFromBaseClass)
	{
		ASSERT_THAT(AreEqual(42, SpecialValue));
	}
};

template <typename Derived, typename AsserterType>
struct TBaseWithConstructor : public TTest<Derived, AsserterType>
{
	static inline bool bInitializedCall{ false };

	static inline bool bNonInitializedCall{ false };

	TBaseWithConstructor()
	{
		if (this->bInitializing)
		{
			bInitializedCall = true;
		}
		else
		{
			bNonInitializedCall = true;
		}
	}
};

#define CUSTOM_WITH_CTOR_CLASS(_ClassName, _TestDir) TEST_CLASS_WITH_BASE(_ClassName, _TestDir, TBaseWithConstructor)

CUSTOM_WITH_CTOR_CLASS(CustomCtor, "TestFramework.CQTest.Core")
{
	TEST_METHOD(CustomClassWithCtor_CallsCtorWhenInitializing_AndBeforeEachTest)
	{
		ASSERT_THAT(IsTrue(bInitializedCall));	 // called when populating test names
		ASSERT_THAT(IsTrue(bNonInitializedCall)); // called when creating this test
	}
};