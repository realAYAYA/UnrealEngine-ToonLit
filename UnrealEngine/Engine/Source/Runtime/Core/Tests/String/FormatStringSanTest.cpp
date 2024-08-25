// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_TESTS

#include "String/FormatStringSan.h"

#include "Tests/TestHarnessAdapter.h"

namespace UE
{
TEST_CASE_NAMED(FFormatStringValidatorTest, "System::Core::String::FormatStringSan", "[Core][String][FormatStringSan]")
{
	using namespace UE::Core::Private;
	SECTION("Error Handling")
	{
		STATIC_CHECK(UE_CHECK_FORMAT_STRING_ERR(FormatStringSan::StatusSNeedsTCHARPtrArg, TEXT("Test %s"), "wrong"));
		STATIC_CHECK(UE_CHECK_FORMAT_STRING_ERR(FormatStringSan::StatusHSNeedsCharPtrArg, TEXT("Test %hs"), TEXT("wrong")));
		STATIC_CHECK(UE_CHECK_FORMAT_STRING_ERR(FormatStringSan::StatusIncompleteFormatSpecifierOrUnescapedPercent, TEXT("Hello %")));
		STATIC_CHECK(UE_CHECK_FORMAT_STRING_ERR(FormatStringSan::StatusCNeedsTCHARArg, TEXT("Hello %c"), char(42)));
		STATIC_CHECK(UE_CHECK_FORMAT_STRING_ERR(FormatStringSan::StatusPNeedsPointerArg, TEXT("Hello %p"), 42));
		STATIC_CHECK(UE_CHECK_FORMAT_STRING_ERR(FormatStringSan::StatusFNeedsFloatOrDoubleArg, TEXT("Hello %f"), 42));
		STATIC_CHECK(std::is_same_v<TCHAR, WIDECHAR> == UE_CHECK_FORMAT_STRING_ERR(FormatStringSan::StatusCapitalSNeedsCharPtr, TEXT("Hello %S"), TEXT("wrong")));
		STATIC_CHECK(std::is_same_v<TCHAR, ANSICHAR> == UE_CHECK_FORMAT_STRING_ERR(FormatStringSan::StatusCapitalSNeedsTCHARPtr, TEXT("Hello %S"), "wrong"));
		STATIC_CHECK(UE_CHECK_FORMAT_STRING_ERR(FormatStringSan::StatusInvalidFormatSpec, TEXT("Hello %k"), 42));
		STATIC_CHECK(UE_CHECK_FORMAT_STRING_ERR(FormatStringSan::StatusNotEnoughArguments, TEXT("Hello %s")));
		STATIC_CHECK(UE_CHECK_FORMAT_STRING_ERR(FormatStringSan::StatusDNeedsIntegerArg, TEXT("Hello %d"), 42.0));
		STATIC_CHECK(UE_CHECK_FORMAT_STRING_ERR(FormatStringSan::StatusZNeedsIntegerSpec, TEXT("Hello %z test"), 42));
		STATIC_CHECK(UE_CHECK_FORMAT_STRING_ERR(FormatStringSan::StatusZNeedsIntegerArg, TEXT("Hello %zu"), "hi"));
		STATIC_CHECK(UE_CHECK_FORMAT_STRING_ERR(FormatStringSan::StatusDynamicLengthSpecNeedsIntegerArg, TEXT("Hey %*.*d"), "hi", "hi"));
		STATIC_CHECK(UE_CHECK_FORMAT_STRING_ERR(FormatStringSan::StatusLNeedsIntegerArg, TEXT("Hello %ld"), 43.0f));
		STATIC_CHECK(UE_CHECK_FORMAT_STRING_ERR(FormatStringSan::StatusHNeedsIntegerArg, TEXT("Hello %hd"), 43.0f));
		STATIC_CHECK(UE_CHECK_FORMAT_STRING_ERR(FormatStringSan::StatusHHNeedsIntegerSpec, TEXT("Hello %hh "), 42));
		STATIC_CHECK(UE_CHECK_FORMAT_STRING_ERR(FormatStringSan::StatusHHNeedsIntegerArg, TEXT("Hello %hhd"), 43.0f));
		STATIC_CHECK(UE_CHECK_FORMAT_STRING_ERR(FormatStringSan::StatusLLNeedsIntegerSpec, TEXT("Hello %ll "), 42));
		STATIC_CHECK(UE_CHECK_FORMAT_STRING_ERR(FormatStringSan::StatusLLNeedsIntegerArg, TEXT("Hello %lld"), 43.0f));
		STATIC_CHECK(UE_CHECK_FORMAT_STRING_ERR(FormatStringSan::StatusI64BadSpec, TEXT("Hello %I32d"), 44));
		STATIC_CHECK(UE_CHECK_FORMAT_STRING_ERR(FormatStringSan::StatusI64BadSpec, TEXT("Hello %I64p"), 44));
		STATIC_CHECK(UE_CHECK_FORMAT_STRING_ERR(FormatStringSan::StatusI64NeedsIntegerArg	, TEXT("Hello %I64u"), 44.0f));
		STATIC_CHECK(UE_CHECK_FORMAT_STRING_ERR(FormatStringSan::StatusInvalidFormatSpec, TEXT("%l^"), 42));
		STATIC_CHECK(UE_CHECK_FORMAT_STRING_ERR(FormatStringSan::StatusInvalidFormatSpec, TEXT("%h^"), 42));
		STATIC_CHECK(UE_CHECK_FORMAT_STRING_ERR(FormatStringSan::StatusIncompleteFormatSpecifierOrUnescapedPercent, TEXT("%-*"), 42));		
	}

	SECTION("Accepted Formatting")
	{
		STATIC_CHECK(UE_CHECK_FORMAT_STRING_ERR(FormatStringSan::StatusOk, TEXT("Test %d %% %% %d"), 32, 44));
		STATIC_CHECK(UE_CHECK_FORMAT_STRING_ERR(FormatStringSan::StatusOk, TEXT("Test")));

		STATIC_CHECK(UE_CHECK_FORMAT_STRING_ERR(FormatStringSan::StatusOk, TEXT("%% Test %d %f %s "), 32, 44.4, TEXT("hey")));
		STATIC_CHECK(UE_CHECK_FORMAT_STRING_ERR(FormatStringSan::StatusOk, TEXT("Test %.3f %d"), 4.4, 2));
		STATIC_CHECK(UE_CHECK_FORMAT_STRING_ERR(FormatStringSan::StatusOk, TEXT("Test %2.3f"), 4.4));
		STATIC_CHECK(UE_CHECK_FORMAT_STRING_ERR(FormatStringSan::StatusOk, TEXT("Test %2.f"), 4.4));
		STATIC_CHECK(UE_CHECK_FORMAT_STRING_ERR(FormatStringSan::StatusOk, TEXT("Test %2f"), 4.4));
		STATIC_CHECK(UE_CHECK_FORMAT_STRING_ERR(FormatStringSan::StatusOk, TEXT("Test %d"), long(32)));
		STATIC_CHECK(UE_CHECK_FORMAT_STRING_ERR(FormatStringSan::StatusOk, TEXT("Test %s"), TEXT("hello")));
		STATIC_CHECK(UE_CHECK_FORMAT_STRING_ERR(FormatStringSan::StatusOk, TEXT("Test percent %% more")));
		STATIC_CHECK(UE_CHECK_FORMAT_STRING_ERR(FormatStringSan::StatusOk, TEXT("Test %hs"), "hi"));

		STATIC_CHECK(UE_CHECK_FORMAT_STRING_ERR(FormatStringSan::StatusOk, TEXT("%-8d %f"), 42, 4.4));
		STATIC_CHECK(UE_CHECK_FORMAT_STRING_ERR(FormatStringSan::StatusOk, TEXT("%8d %f"), 42, 4.4));

		STATIC_CHECK(UE_CHECK_FORMAT_STRING_ERR(FormatStringSan::StatusOk, TEXT("%-8.8d %f"), 42, 4.4));
		STATIC_CHECK(UE_CHECK_FORMAT_STRING_ERR(FormatStringSan::StatusOk, TEXT("%hhd %d"), int(42), 42));
		STATIC_CHECK(UE_CHECK_FORMAT_STRING_ERR(FormatStringSan::StatusOk, TEXT("%lld %d"), 42LL, 42));
		STATIC_CHECK(UE_CHECK_FORMAT_STRING_ERR(FormatStringSan::StatusOk, TEXT("%I64d %d "), 42LL, 42));
		STATIC_CHECK(UE_CHECK_FORMAT_STRING_ERR(FormatStringSan::StatusOk, TEXT("%d"), size_t(44)));

		STATIC_CHECK(UE_CHECK_FORMAT_STRING_ERR(FormatStringSan::StatusOk, TEXT("%f"), 42.f));

		STATIC_CHECK(UE_CHECK_FORMAT_STRING_ERR(FormatStringSan::StatusOk, TEXT("%-*.*d %f"), 4, 8, 42, 4.4));
		STATIC_CHECK(UE_CHECK_FORMAT_STRING_ERR(FormatStringSan::StatusOk, TEXT("%*.*d %f"), 4, 8, 42, 4.4));
		STATIC_CHECK(UE_CHECK_FORMAT_STRING_ERR(FormatStringSan::StatusOk, TEXT("%-*.9d $d"), 4, 42, 44));
		STATIC_CHECK(UE_CHECK_FORMAT_STRING_ERR(FormatStringSan::StatusOk, TEXT("%*.9d $d"), 4, 42, 44));
		STATIC_CHECK(UE_CHECK_FORMAT_STRING_ERR(FormatStringSan::StatusOk, TEXT("%-*d %s"), 4, 42, TEXT("hi")));
		STATIC_CHECK(UE_CHECK_FORMAT_STRING_ERR(FormatStringSan::StatusOk, TEXT("%*d %s"), 4, 42, TEXT("hi")));
		STATIC_CHECK(UE_CHECK_FORMAT_STRING_ERR(FormatStringSan::StatusOk, TEXT("%.*f %s"), 4, 42.4, TEXT("hi")));

		STATIC_CHECK(UE_CHECK_FORMAT_STRING_ERR(FormatStringSan::StatusOk, TEXT("Test extra arg '%s'."), TEXT("ok"), TEXT("hi")));

		STATIC_CHECK(UE_CHECK_FORMAT_STRING_ERR(FormatStringSan::StatusOk, TEXT("a")));
		STATIC_CHECK(UE_CHECK_FORMAT_STRING_ERR(FormatStringSan::StatusOk, TEXT("")));

		{
			const TCHAR* Foo;
			STATIC_CHECK(UE_CHECK_FORMAT_STRING_ERR(FormatStringSan::StatusOk, TEXT("hello %s"), Foo));
		}
		{
			enum MyIntegralEnum { MyIntegralEnumA };
			STATIC_CHECK(UE_CHECK_FORMAT_STRING_ERR(FormatStringSan::StatusOk, TEXT("hello %d is an enum actually"), MyIntegralEnumA));
		}
		{
			enum class MyEnum { Value };
			STATIC_CHECK(UE_CHECK_FORMAT_STRING_ERR(FormatStringSan::StatusOk, TEXT("enum class %d value"), MyEnum::Value));
		}

		{
			enum ETestEnumAsByte { ETestEnumAsByte_Zero = 0 };
			TEnumAsByte<ETestEnumAsByte> EnumAsByteParam = ETestEnumAsByte_Zero;
			STATIC_CHECK(UE_CHECK_FORMAT_STRING_ERR(FormatStringSan::StatusOk, TEXT("%d"), EnumAsByteParam));
		}
	}
}

TEST_CASE_NAMED(FFormatStringConstStringValidationTest, "System::Core::String::FormatStringSan::ConstString", "[Core][String][FormatStringSan]")
{
	using namespace UE::Core::Private::FormatStringSan;

	SECTION("Valid Const String Conditions")
	{
		STATIC_CHECK(bIsAConstString<decltype("Raw CString")>);
		STATIC_CHECK(bIsAConstString<decltype(TEXT("Raw WString"))>);

		{
			const char Array[] = "CString";
			STATIC_CHECK(bIsAConstString<decltype(Array)>);
		}

		{
			const TCHAR Array[] = TEXT("WString");
			STATIC_CHECK(bIsAConstString<decltype(Array)>);
		}

		{
			const char* ConstPtr = "CString";
			STATIC_CHECK(bIsAConstString<decltype(ConstPtr)>);
		}

		{
			const TCHAR* ConstPtr = TEXT("WString");
			STATIC_CHECK(bIsAConstString<decltype(ConstPtr)>);
		}

		{
			const char* const ConstPtrConst = "CString";
			STATIC_CHECK(bIsAConstString<decltype(ConstPtrConst)>);
		}

		{
			const TCHAR* const ConstPtrConst = TEXT("WString");
			STATIC_CHECK(bIsAConstString<decltype(ConstPtrConst)>);
		}
		
		{
			struct FImplicitConvertToChar
			{
				operator const char*() const { return (const char*)this; }
			};
			FImplicitConvertToChar ToChar;
			STATIC_CHECK(bIsAConstString<decltype(ToChar)>);
		}

		{
			struct FImplicitConvertToTChar
			{
				operator const TCHAR* () const { return (const TCHAR*)this; }
			};
			FImplicitConvertToTChar ToTChar;
			STATIC_CHECK(bIsAConstString<decltype(ToTChar)>);
		}
	}

	SECTION("Invalid Const String Conditions")
	{
		bool bBool = true;
		STATIC_CHECK_FALSE(bIsAConstString<decltype(bBool)>);

		{
			char* Ptr = nullptr;
			STATIC_CHECK_FALSE(bIsAConstString<decltype(Ptr)>);
		}

		{
			TCHAR* Ptr = nullptr;
			STATIC_CHECK_FALSE(bIsAConstString<decltype(Ptr)>);
		}

		{
			char* const Ptr = nullptr;
			STATIC_CHECK_FALSE(bIsAConstString<decltype(Ptr)>);
		}

		{
			TCHAR* const Ptr = nullptr;
			STATIC_CHECK_FALSE(bIsAConstString<decltype(Ptr)>);
		}

		{
			struct FExplicitConvertToChar
			{
				explicit operator const char* () const { return (const char*)this; }
			};
			FExplicitConvertToChar ToChar;
			STATIC_CHECK_FALSE(bIsAConstString<decltype(ToChar)>);
		}

		{
			struct FExplicitConvertToTChar
			{
				explicit operator const TCHAR* () const { return (const TCHAR*)this; }
			};
			FExplicitConvertToTChar ToTChar;
			STATIC_CHECK_FALSE(bIsAConstString<decltype(ToTChar)>);
		}
	}
}

} // namespace UE

#endif // WITH_TESTS
