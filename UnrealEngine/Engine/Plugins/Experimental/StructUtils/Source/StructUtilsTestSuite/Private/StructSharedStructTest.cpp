// Copyright Epic Games, Inc. All Rights Reserved.

#include "AITestsCommon.h"
#include "SharedStruct.h"
#include "StructUtilsTestTypes.h"

#define LOCTEXT_NAMESPACE "StructUtilsTests"

UE_DISABLE_OPTIMIZATION_SHIP

namespace FStructSharedStructTest
{
	struct FTest_SharedStructCreate : FAITestBase
	{
		virtual bool InstantTest() override
		{
			{
				FSharedStruct SharedStruct = FSharedStruct::Make<FTestStructSimpleNonZeroDefault>();

				AITEST_EQUAL("FSharedStruct default initalized from Make should have same value as default constructed", FTestStructSimpleNonZeroDefault(), SharedStruct.Get<FTestStructSimpleNonZeroDefault>());
			}

			{
				const float Val = 99.f;
				FTestStructSimple Simple(Val);

				FSharedStruct SharedStruct = FSharedStruct::Make(Simple);

				AITEST_EQUAL("FSharedStruct initiazed from Make should have value of FTestStructSimple its initiliazed from", Val, SharedStruct.Get<FTestStructSimple>().Float);
			}

			{
				const float Val = 99.f;
				FTestStructSimple Simple(Val);

				FSharedStruct SharedStruct = FSharedStruct::Make(FTestStructSimple::StaticStruct(), (uint8*)&Simple);

				AITEST_EQUAL("FSharedStruct initiazed from Make should have value of StaticStruct and memory its initiliazed from", Val, SharedStruct.Get<FTestStructSimple>().Float);
			}

			{
				const float Val = 99.f;
				FTestStructSimple Simple(Val);
				FStructView StructView = FStructView::Make(Simple);
				FSharedStruct SharedStruct = FSharedStruct::Make(StructView);

				AITEST_EQUAL("FSharedStruct initiazed from Make should have value of FStructView its initiliazed from", Val, SharedStruct.Get<const FTestStructSimple>().Float);
			}

			{
				const float Val = 99.f;
				FTestStructSimple Simple(Val);

				FSharedStruct SharedStruct = FSharedStruct::Make<FTestStructSimple>(99);

				AITEST_EQUAL("FSharedStruct initiazed from Make should have value reflecting TArgs", Val, SharedStruct.Get<FTestStructSimple>().Float);
			}

			return true;
		}
	};

	IMPLEMENT_AI_INSTANT_TEST(FTest_SharedStructCreate, "System.StructUtils.SharedStruct.Make");

	struct FTest_SharedStructBasic : FAITestBase
	{
		virtual bool InstantTest() override
		{
			{
				FSharedStruct SharedStruct = FSharedStruct::Make<FTestStructSimple>();
				FSharedStruct SharedStruct2(SharedStruct);

				AITEST_EQUAL("FSharedStruct and FSharedStruct should be equal from copy construction", SharedStruct, SharedStruct2);
			}

			{
				FSharedStruct SharedStruct = FSharedStruct::Make<FTestStructSimple>();
				FSharedStruct SharedStruct2;

				SharedStruct2 = SharedStruct;

				AITEST_EQUAL("FSharedStruct and FSharedStruct should be equal from copy asignment", SharedStruct, SharedStruct2);
			}

			{
				const float Val = 99.f;

				FSharedStruct SharedStruct = FSharedStruct::Make<FTestStructSimple>();
				FSharedStruct SharedStruct2(SharedStruct);

				SharedStruct.Get<FTestStructSimple>().Float = Val;

				AITEST_EQUAL("SharedStruct and SharedStruct2 should reflect the same changes to shared FTestStructSimple", SharedStruct.Get<FTestStructSimple>().Float, SharedStruct2.Get<FTestStructSimple>().Float);
			}

			{
				FSharedStruct SharedStruct;
				AITEST_FALSE("Default constructed FSharedStruct should IsValid() == false", SharedStruct.IsValid());
			}

			{
				FSharedStruct SharedStruct = FSharedStruct::Make<FTestStructSimple>();
				AITEST_TRUE("FSharedStruct created to a specific struct type should be IsValid()", SharedStruct.IsValid());
			}

			return true;
		}
	};

	IMPLEMENT_AI_INSTANT_TEST(FTest_SharedStructBasic, "System.StructUtils.SharedStruct.Basic");

	struct FTest_ConstSharedStructCreate : FAITestBase
	{
		virtual bool InstantTest() override
		{
			{
				FConstSharedStruct SharedStruct = FConstSharedStruct::Make<FTestStructSimpleNonZeroDefault>();

				AITEST_EQUAL("FConstSharedStruct default initalized from Make should have same value as default constructed", FTestStructSimpleNonZeroDefault(), SharedStruct.Get<const FTestStructSimpleNonZeroDefault>());
			}

			{
				const float Val = 99.f;
				FTestStructSimple Simple(Val);

				FConstSharedStruct SharedStruct = FConstSharedStruct::Make(Simple);

				AITEST_EQUAL("FConstSharedStruct initiazed from Make should have value of FTestStructSimple its initiliazed from", Val, SharedStruct.Get<const FTestStructSimple>().Float);
			}

			{
				const float Val = 99.f;
				FTestStructSimple Simple(Val);

				FConstSharedStruct ConstSharedStruct = FConstSharedStruct::Make(FTestStructSimple::StaticStruct(), (uint8*)&Simple);

				AITEST_EQUAL("FConstSharedStruct initiazed from Make should have value of StaticStruct and memory its initiliazed from", Val, ConstSharedStruct.Get<const FTestStructSimple>().Float);
			}

			{
				const float Val = 99.f;
				FTestStructSimple Simple(Val);
				FStructView StructView = FStructView::Make(Simple);
				FConstSharedStruct ConstSharedStruct = FConstSharedStruct::Make(StructView);

				AITEST_EQUAL("FConstSharedStruct initiazed from Make should have value of FStructView its initiliazed from", Val, ConstSharedStruct.Get<const FTestStructSimple>().Float);
			}

			{
				const float Val = 99.f;
				FTestStructSimple Simple(Val);

				FConstSharedStruct ConstSharedStruct = FConstSharedStruct::Make<FTestStructSimple>(99);

				AITEST_EQUAL("FConstSharedStruct initiazed from Make should have value reflecting TArgs", Val, ConstSharedStruct.Get<const FTestStructSimple>().Float);
			}

			return true;
		}
	};

	IMPLEMENT_AI_INSTANT_TEST(FTest_ConstSharedStructCreate, "System.StructUtils.SharedStruct.Const.Make");

	struct FTest_ConstSharedStructBasic : FAITestBase
	{
		virtual bool InstantTest() override
		{
			{
				FConstSharedStruct ConstSharedStruct = FConstSharedStruct::Make<FTestStructSimple>();
				FConstSharedStruct ConstSharedStruct2(ConstSharedStruct);

				AITEST_EQUAL("FConstSharedStruct and FConstSharedStruct should be equal from copy construction", ConstSharedStruct, ConstSharedStruct2);
			}

			{
				FConstSharedStruct ConstSharedStruct = FConstSharedStruct::Make<FTestStructSimple>();
				FConstSharedStruct ConstSharedStruct2;
				
				ConstSharedStruct2 = ConstSharedStruct;

				AITEST_EQUAL("FConstSharedStruct and FConstSharedStruct should be equal from copy asignment", ConstSharedStruct, ConstSharedStruct2);
			}

			{
				FSharedStruct SharedStruct = FSharedStruct::Make<FTestStructSimple>();
				FConstSharedStruct ConstSharedStruct(SharedStruct);

				AITEST_TRUE("FSharedStruct and FConstSharedStruct should be equal from copy construction", SharedStruct == ConstSharedStruct);
			}

			{
				FSharedStruct SharedStruct = FSharedStruct::Make<FTestStructSimple>();
				FConstSharedStruct ConstSharedStruct;
				
				ConstSharedStruct = SharedStruct;

				AITEST_TRUE("FSharedStruct and FConstSharedStruct should be equal from copy asignment", SharedStruct == ConstSharedStruct);
			}

			{
				const float Val = 99.f;

				FSharedStruct SharedStruct = FSharedStruct::Make<FTestStructSimple>();
				FConstSharedStruct ConstSharedStruct(SharedStruct);

				SharedStruct.Get<FTestStructSimple>().Float = Val;

				AITEST_EQUAL("FSharedStruct and FConstSharedStruct should reflect the same changes to shared FTestStructSimple", SharedStruct.Get<FTestStructSimple>().Float, ConstSharedStruct.Get<const FTestStructSimple>().Float);
			}

			{
				FConstSharedStruct ConstSharedStruct;
				AITEST_FALSE("Default constructed FConstSharedStruct should IsValid() == false", ConstSharedStruct.IsValid());
			}

			{
				FConstSharedStruct ConstSharedStruct = FConstSharedStruct::Make<FTestStructSimple>();
				AITEST_TRUE("FConstSharedStruct created to a specific struct type should be IsValid()", ConstSharedStruct.IsValid());
			}

			return true;
		}
	};

	IMPLEMENT_AI_INSTANT_TEST(FTest_ConstSharedStructBasic, "System.StructUtils.SharedStruct.Const.Basic");
}


UE_ENABLE_OPTIMIZATION_SHIP

#undef LOCTEXT_NAMESPACE
