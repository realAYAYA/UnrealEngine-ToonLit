// Copyright Epic Games, Inc. All Rights Reserved.

#include "AITestsCommon.h"
#include "StructArrayView.h"
#include "StructUtilsTestTypes.h"

#define LOCTEXT_NAMESPACE "StructUtilsTests"

PRAGMA_DISABLE_OPTIMIZATION

namespace FStructArrayViewTest
{

struct FTest_ChangeElements : FAITestBase
{
	virtual bool InstantTest() override
	{
		constexpr float Increment = 10.f;
		TArray<float> OriginalValues = { 1,2,3,4,5,6,7,8,9,10 };
		TArray<FTestStructSimple> ValuesF;
		for (float Value : OriginalValues)
		{
			ValuesF.Add(Value);
		}
		FStructArrayView ViewF = FStructArrayView(ValuesF);

		for (int i = 0; i < ValuesF.Num(); ++i)
		{
			ViewF.GetMutableElementAt<FTestStructSimple>(i).Float += 10.f;
		}

		for (int i = 0; i < ValuesF.Num(); ++i)
		{
			AITEST_EQUAL(TEXT("Element should have the same value independently of access method"), ValuesF[i].Float, ViewF.GetElementAt<FTestStructSimple>(i).Float);
			AITEST_EQUAL(TEXT("Element should reflect the change done to it"), ViewF.GetElementAt<FTestStructSimple>(i).Float, OriginalValues[i] + Increment);
			AITEST_EQUAL(TEXT("Element should point at the same adress in memory, independently of access method"), (void*)&ValuesF[i], ViewF.GetDataAt(i));
		}

		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FTest_ChangeElements, "System.StructUtils.StructArrayView.ChangeElements");

} // FStructArrayViewTest

PRAGMA_ENABLE_OPTIMIZATION

#undef LOCTEXT_NAMESPACE
