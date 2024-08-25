// Copyright Epic Games, Inc. All Rights Reserved.

#include "UObject/PropertyTypeName.h"
#include "UObject/WeakFieldPtr.h"
#include "UObject/UnrealType.h"

#if WITH_TESTS

#include "Tests/TestHarnessAdapter.h"

namespace UE
{

TEST_CASE_NAMED(TWeakFieldPtrSmokeTest, "CoreUObject::TWeakFieldPtr::Smoke", "[Core][UObject][SmokeFilter]")
{
	SECTION("Check that TWeakFieldPtr can hold 'const FProperty'")
	{
		const FProperty* PropPtr = nullptr;
		TWeakFieldPtr<const FProperty> ConstPtr1;
		TWeakFieldPtr<const FProperty> ConstPtr2;

		ConstPtr1 = PropPtr;
		ConstPtr1 = ConstPtr2;

		// validate that the static_asserts inside these operators allow const FProperty as T
		CHECK(ConstPtr1 == ConstPtr2);
		CHECK(!(ConstPtr1 != ConstPtr2));
		CHECK(ConstPtr1 == PropPtr);
		CHECK(!(ConstPtr1 != PropPtr));
	}
}

} // UE

#endif // WITH_TESTS
